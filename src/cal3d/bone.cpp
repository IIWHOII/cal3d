//****************************************************************************//
// bone.cpp                                                                   //
// Copyright (C) 2001, 2002 Bruno 'Beosil' Heidelberger                       //
//****************************************************************************//
// This library is free software; you can redistribute it and/or modify it    //
// under the terms of the GNU Lesser General Public License as published by   //
// the Free Software Foundation; either version 2.1 of the License, or (at    //
// your option) any later version.                                            //
//****************************************************************************//

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cal3d/error.h"
#include "cal3d/bone.h"
#include "cal3d/coremesh.h"
#include "cal3d/coresubmesh.h"
#include "cal3d/corebone.h"
#include "cal3d/matrix.h"
#include "cal3d/skeleton.h"
#include "cal3d/coreskeleton.h"


CalBone::CalBone(const CalCoreBone& coreBone)
    : parentId(coreBone.parentId)
    , coreRelativeTransform(coreBone.relativeTransform)
    , coreBoneSpaceTransform(coreBone.boneSpaceTransform)
{
    clearState();
}

void CalBone::clearState() {
    m_accumulatedWeight = 0.0f;
    m_accumulatedWeightAbsolute = 0.0f;
    m_accumulatedReplacementAttenuation = 1.0f;
    m_meshScaleAbsolute.set(1, 1, 1);
}

/*****************************************************************************/
/** Interpolates the current state to another state.
  *
  * This function interpolates the current state (relative translation and
  * rotation) of the bone instance to another state of a given weight.
  *
  * @param replace If true, subsequent animations will have their weight attenuated by 1 - rampValue.
  * @param rampValue Amount to attenuate weight when ramping in/out the animation.
  *****************************************************************************/

void CalBone::blendState(
    const cal3d::Transform& transform,
    bool replace,
    const float rampValue
) {
    // Attenuate the weight by the accumulated replacement attenuation.  Each applied
    // "replacement" animation attenuates the weights of the subsequent animations by
    // the inverse of its rampValue, so that when a replacement animation ramps up to
    // full, all lesser priority animations automatically ramp down to zero.
    const float attenuatedWeight = rampValue * m_accumulatedReplacementAttenuation;
    if (replace) {
        m_accumulatedReplacementAttenuation *= (1.0f - rampValue);
    }

    bool first = m_accumulatedWeightAbsolute == 0.0f;
    m_accumulatedWeightAbsolute += attenuatedWeight;
        
    // Now apply weighted, scaled transformation.  For weights, Cal starts with the
    // first and then blends the later ones in proportion to their weights.  Though this
    // would seem to depend on the order, you can reason by induction that it does not.
    // Each application of an animation gives it the correct proportion to the others in
    // aggregate and leaves in tact the proportions among the others.
    if (first) {

        // It is the first state, so we can just copy it into the bone state.  The first animation
        // must be applied with scale = 1.0 since it is the initial pose rather than something
        // to be blended onto a pose.  If we scale the first state, the skeleton will look like
        // a crumpled spider.
        absoluteTransform = transform;
    } else {

        // Consider an example with two animations, one or both of them "replace" animations.
        // Wave is a "replace" animation, played on top of Walk.  Wave is applied first since it is a
        // "replace" animation and Walk is not.  Imagine Wave is ramping in, currently at 80%.  Wave sets
        // the initial pose 100% and then Walk is applied over that pose with a blend factor of 0.2.  The result
        // is that Wave is 80% and Walk is 20%, which is what you'd expect for replace semantics.
        //
        // Animation    RampedWeight  AttenuatedWeight    InAccumWeightAbs  OutAccAttenuation   Factor
        // Wave         0.8           0.8                 0.0               0.2 (replace)       n/a (100%)
        // Walk         1.0           0.2                 0.8               0.2 (not replace)   0.2/(0.8+0.2) = 0.2
        //
        // Consider the same example with two animations, but neither of them "replace" animations.
        // Assume Wave is applied first.  Imagine Wave is ramping in, currently at 80%.  Wave sets
        // the initial pose 100% and then Walk is applied over that pose with a blend factor of 0.55.  The result
        // is that Wave is 45% and Walk is 55%, which is about what you'd expect for non-replace semantics.
        //
        // Animation    RampedWeight  AttenuatedWeight    InAccumWeightAbs  OutAccAttenuation   Factor
        // Wave         0.8           0.8                 0.0               1.0 (not replace)   n/a (100%)
        // Walk         1.0           1.0                 0.8               1.0 (not replace)   1.0/(0.8+1.0) = 0.55
        //
        // Consider the same example again but reverse the order of Wave and Walk, so Walk is applied first.
        // As before, imagine Wave is ramping in, currently at 80%.  Walk sets the initial pose 100%
        // and then Wave is applied over that pose with a blend factor of 0.44.  The result
        // is that Wave is 44% and Walk is 56%, which is also about what you'd expect for non-replace semantics.
        //
        // Animation    RampedWeight  AttenuatedWeight    InAccumWeightAbs  OutAccAttenuation   Factor
        // Walk         1.0           1.0                 0.0               1.0 (not replace)   n/a (100%)
        // Wave         0.8           0.8                 1.0               1.0 (not replace)   0.8/(0.8+1.0) = 0.44
        //
        // Now consider an example in which Point and Wave are both applied over Walk, with Point applied
        // first at highest priority.  Assume that Point is ramped at 90% and Wave is ramped at 80%.  Both
        // Point and Wave are "replace" animations.  Walk is not.  The result is Walk is 2%, Wave is about 8%,
        // and Point is about 90%, which seems like a reasonable result.
        //
        // Animation    RampedWeight  AttenuatedWeight    InAccumWeightAbs  OutAccAttenuation   Factor
        // Point        0.9           0.9                 0                 0.1 (replace)       n/a (100%)
        // Wave         0.8           0.08                0.9               0.02 (replace)      0.08/(0.9+0.08) = 0.082
        // Walk         1.0           0.02                0.98              0.02 (not replace)  0.02/(0.98+0.02) = 0.02
        //
        // Finally, consider an example in which Point and Wave are both applied over Walk, but in which
        // none of the animations is a "replace" animation.  For this example, assume that Point, Wave,
        // and Walk all are fully ramped in at 100%.  The result is Walk is 33%, Wave is about 33%,
        // and Point is about 33%, which seems like the right result.
        //
        // Animation    RampedWeight  AttenuatedWeight    InAccumWeightAbs  OutAccAttenuation   Factor
        // Point        1.0           1.0                 0.0               1.0 (not replace)   n/a (100%)
        // Wave         1.0           1.0                 1.0               1.0 (not replace)   1.0/(1.0+1.0) = 0.5
        // Walk         1.0           1.0                 2.0               1.0 (not replace)   1.0/(1.0+2.0) = 0.33
        float factor = attenuatedWeight / m_accumulatedWeightAbsolute;

        // If the scale of the first blend was not 1.0, then I will adjust the factor of the second blend
        // to compensate,
        //
        //      factor' = 1 - m_firstBlendScale * ( 1 - factor )
        //
        assert(factor <= 1.0f);
        absoluteTransform = blend(factor, absoluteTransform, transform);
    }
}

/*****************************************************************************/
/** Calculates the current state.
  *
  * This function calculates the current state (absolute translation and
  * rotation, as well as the bone space transformation) of the bone instance
  * and all its children.
  *****************************************************************************/

BoneTransform CalBone::calculateState(const CalBone* bones) {
    // === What does lockState() mean?  Why do we need it at all?  It seems only to allow us
    // to blend all the animation actions together into a temporary sum, and then
    // blend all the animation cycles together into a different sum, and then blend
    // the two sums together according to their relative weight sums.  I believe this is mathematically
    // equivalent of blending all the animation actions and cycles together into a single sum,
    // according to their relative weights.

    // clamp accumulated weight
    if (m_accumulatedWeightAbsolute > 1.0f - m_accumulatedWeight) {
        m_accumulatedWeightAbsolute = 1.0f - m_accumulatedWeight;
    }

    if (m_accumulatedWeightAbsolute > 0.0f) {
        if (m_accumulatedWeight == 0.0f) {
            // it is the first state, so we can just copy it into the bone state
            relativeTransform = absoluteTransform;
            m_accumulatedWeight = m_accumulatedWeightAbsolute;
        } else {
            // it is not the first state, so blend all attributes
            float factor = m_accumulatedWeightAbsolute / (m_accumulatedWeight + m_accumulatedWeightAbsolute);

            relativeTransform = blend(factor, relativeTransform, absoluteTransform);

            m_accumulatedWeight += m_accumulatedWeightAbsolute;
        }

        m_accumulatedWeightAbsolute = 0.0f;
    }

    // check if the bone was not touched by any active animation
    if (m_accumulatedWeight == 0.0f) {
        // set the bone to the initial skeleton state
        relativeTransform = coreRelativeTransform;
    }

    if (parentId == -1) {
        // no parent, this means absolute state == relative state
        absoluteTransform = relativeTransform;
    } else {
        absoluteTransform = bones[parentId].absoluteTransform * relativeTransform;
    }

    // calculate the bone space transformation
    CalVector translationBoneSpace(coreBoneSpaceTransform.translation);

    // Must go before the *= m_rotationAbsolute.
    bool meshScalingOn = m_meshScaleAbsolute.x != 1 || m_meshScaleAbsolute.y != 1 || m_meshScaleAbsolute.z != 1;
    if (meshScalingOn) {
        // The mesh transformation is intended to apply to the vector from the
        // bone node to the vert, relative to the model's global coordinate system.
        // For example, even though the head node's X axis aims up, the model's
        // global coordinate system has X to stage right, Z up, and Y stage back.
        //
        // The standard vert transformation is:
        // v1 = vmesh - boneAbsPosInJpose
        // v2 = v1 * boneAbsRotInAnimPose
        // v3 = v2 + boneAbsPosInAnimPose
        //
        // Cal3d does the calculation by:
        // u1 = umesh * transformMatrix
        // u2 = u1 + translationBoneSpace
        //
        // where translationBoneSpace =
        //   "coreBoneTranslationBoneSpace"
        //   * boneAbsRotInAnimPose
        //   + boneAbsPosInAnimPose
        //
        // and where transformMatrix =
        //   "coreBoneRotBoneSpace"
        //   * boneAbsRotInAnimPose
        //
        // I don't know what "coreBoneRotBoneSpace" and "coreBoneTranslationBoneSpace" actually are,
        // but to add scale to the scandard vert transformation, I simply do:
        //
        // v3' = vmesh           * scalevec    * boneAbsRotInAnimPose
        //   - boneAbsPosInJpose * scalevec    * boneAbsRotInAnimPose
        //   + boneAbsPosInAnimPose
        //
        // Essentially, the boneAbsPosInJpose is just an extra vector added to
        // each vertex that we want to subtract out.  We must transform the extra
        // vector in exactly the same way we transform the vmesh.  Therefore if we scale the mesh, we
        // must also scale the boneAbsPosInJpose.
        //
        // Expanding out the u2 equation, we have:
        //
        // u2 = umesh * "coreBoneRotBoneSpace"   * boneAbsRotInAnimPose
        //   + "coreBoneTranslationBoneSpace"    * boneAbsRotInAnimPose
        //   + boneAbsPosInAnimPose
        //
        // We assume that "coreBoneTranslationBoneSpace" = vectorThatMustBeSubtractedFromUmesh * "coreBoneRotBoneSpace":
        //
        // u2 = umesh * "coreBoneRotBoneSpace"                                 * boneAbsRotInAnimPose
        //   + vectorThatMustBeSubtractedFromUmesh * "coreBoneRotBoneSpace"    * boneAbsRotInAnimPose
        //   + boneAbsPosInAnimPose
        //
        // We assume that scale should be applied to umesh, not umesh * "coreBoneRotBoneSpace":
        //
        // u2 = umesh * scaleVec * "coreBoneRotBoneSpace" * boneAbsRotInAnimPose
        //   + "coreBoneTranslationBoneSpace" * "coreBoneRotBoneSpaceInverse" * scaleVec * "coreBoneRotBoneSpace" * boneAbsRotInAnimPose
        //   + boneAbsPosInAnimPose
        //
        // which yields,
        //
        // transformMatrix' =  scaleVec * "coreBoneRotBoneSpace" * boneAbsRotInAnimPose
        //
        // and,
        //
        // translationBoneSpace' =
        //   coreBoneTranslationBoneSpace * "coreBoneRotBoneSpaceInverse" * scaleVec * "coreBoneRotBoneSpace"
        //   * boneAbsRotInAnimPose
        //   + boneAbsPosInAnimPose

        translationBoneSpace = coreBoneSpaceTransform.rotation * ((-coreBoneSpaceTransform.rotation * translationBoneSpace) * m_meshScaleAbsolute);
    }

    CalMatrix transformMatrix(coreBoneSpaceTransform.rotation);
    if (meshScalingOn) {

        // By applying each scale component to the row, instead of the column, we
        // are effectively making the scale apply prior to the rotationBoneSpace.
        transformMatrix.dxdx *= m_meshScaleAbsolute.x;
        transformMatrix.dydx *= m_meshScaleAbsolute.x;
        transformMatrix.dzdx *= m_meshScaleAbsolute.x;

        transformMatrix.dxdy *= m_meshScaleAbsolute.y;
        transformMatrix.dydy *= m_meshScaleAbsolute.y;
        transformMatrix.dzdy *= m_meshScaleAbsolute.y;

        transformMatrix.dxdz *= m_meshScaleAbsolute.z;
        transformMatrix.dydz *= m_meshScaleAbsolute.z;
        transformMatrix.dzdz *= m_meshScaleAbsolute.z;
    }

    return BoneTransform(
        CalMatrix(absoluteTransform.rotation) * transformMatrix,
        absoluteTransform * translationBoneSpace);
}
