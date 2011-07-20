//****************************************************************************//
// submesh.cpp                                                                //
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

#include <string>
#include <boost/static_assert.hpp>
#include "cal3d/submesh.h"
#include "cal3d/error.h"
#include "cal3d/coresubmesh.h"
#include "cal3d/coresubmorphtarget.h"

BOOST_STATIC_ASSERT(sizeof(CalIndex) == 2);

// For Exclusive type morph targets, we record a replacement attenuation after
// encountering the first Replace blend.  Until then, we recognize that we do
// not yet have a value by setting this field to this specific invalid value.
static float const ReplacementAttenuationNull = 100.0; // Any number not between zero and one.

CalSubmesh::CalSubmesh(const boost::shared_ptr<CalCoreSubmesh>& pCoreSubmesh)
    : coreSubmesh(pCoreSubmesh) {
    assert(pCoreSubmesh);

    morphTargetWeights.resize(coreSubmesh->getCoreSubMorphTargetCount());
    m_vectorAccumulatedWeight.resize(coreSubmesh->getCoreSubMorphTargetCount());
    m_vectorReplacementAttenuation.resize(coreSubmesh->getCoreSubMorphTargetCount());

    for (int morphTargetId = 0; morphTargetId < coreSubmesh->getCoreSubMorphTargetCount(); ++morphTargetId) {
        morphTargetWeights[morphTargetId] = 0.0f;
        m_vectorAccumulatedWeight[morphTargetId] = 0.0f;
        m_vectorReplacementAttenuation[morphTargetId] = ReplacementAttenuationNull;
    }
}

void CalSubmesh::setMorphTargetWeight(std::string const& morphName, float weight) {
    for (size_t i = 0; i < morphTargetWeights.size(); i++) {
        const boost::shared_ptr<CalCoreSubMorphTarget>& target = coreSubmesh->getCoreSubMorphTarget(i);
        if (target->name == morphName) {
            morphTargetWeights[i] = weight;
            return;
        }
    }
}

void CalSubmesh::clearMorphTargetScales() {
    size_t size = morphTargetWeights.size();
    for (size_t i = 0; i < size; i++) {
        morphTargetWeights[i] = 0.0f;
        m_vectorAccumulatedWeight[ i ] = 0.0f;
        m_vectorReplacementAttenuation[ i ] = ReplacementAttenuationNull;
    }
}


void CalSubmesh::clearMorphTargetState(std::string const& morphName) {
    for (size_t i = 0; i < morphTargetWeights.size(); i++) {
        const boost::shared_ptr<CalCoreSubMorphTarget>& target = coreSubmesh->getCoreSubMorphTarget(i);
        if (target->name == morphName) {
            morphTargetWeights[i] = 0.0f;
            m_vectorAccumulatedWeight[ i ] = 0.0f;
            m_vectorReplacementAttenuation[ i ] = ReplacementAttenuationNull;
        }
    }
}


void CalSubmesh::blendMorphTargetScale(
    std::string const& morphName,
    float scale,
    float unrampedWeight,
    float rampValue,
    bool replace
) {
    size_t size = morphTargetWeights.size();
    for (size_t i = 0; i < size; i++) {
        const boost::shared_ptr<CalCoreSubMorphTarget>& target = coreSubmesh->getCoreSubMorphTarget(i);
        if (target->name == morphName) {
            CalMorphTargetType mtype = target->morphTargetType;
            switch (mtype) {
                case CalMorphTargetTypeAdditive: {

                    // Actions affecting the same morph target channel add their ramped scales
                    // if the channel is Additive.  The unrampedWeight parameter is ignored
                    // because the actions are not affecting each other so there is no need
                    // to assign them a relative weight.
                    morphTargetWeights[ i ] += scale * rampValue;
                    break;
                }
                case CalMorphTargetTypeClamped: {

                    // Like Additive, but clamped to 1.0.
                    morphTargetWeights[ i ] += scale * rampValue;
                    if (morphTargetWeights[ i ] > 1.0) {
                        morphTargetWeights[ i ] = 1.0;
                    }
                    break;
                }
                case CalMorphTargetTypeExclusive:
                case CalMorphTargetTypeAverage: {

                    float attenuatedWeight = unrampedWeight * rampValue;

                    // Each morph target is having multiple actions blended into it.  The composition mode (e.g., exclusive)
                    // is a property of the morph target itself, so you don't ever get an exclusive blend competing with
                    // an average blend, for example.  You get different actions all blending into the same morph target.

                    // For morphs of the Exclusive type, I pick one of the Replace actions arbitrarily
                    // and attenuate all the other actions' influence by the inverse of the Replace action's
                    // rampValue.  If I don't have a Replace action, then the result is the same as the
                    // Average type morph target.  This procedure is not exactly the same as the skeletal animation
                    // Replace composition function.  The skeletal animation Replace function supports combined
                    // attenuation of multiple Replace animations, whereas morph animation Exclusive type
                    // supports only one Replace morph animation, arbitrarily chosen, to attenuate the other
                    // animations.  The reason for the difference is that skeletal animations are sorted in
                    // the mixer, and morph animations are in an arbitrary order.
                    //
                    // If I already have a Replace chosen, then I attenuate this action.
                    // Otherwise, if this action is a Replace, then I record it and attenuate current scale.
                    if (mtype == CalMorphTargetTypeExclusive) {
                        if (m_vectorReplacementAttenuation[ i ] != ReplacementAttenuationNull) {
                            attenuatedWeight *= m_vectorReplacementAttenuation[ i ];
                        } else {
                            if (replace) {
                                float attenuation = 1.0f - rampValue;
                                m_vectorReplacementAttenuation[ i ] = attenuation;
                                morphTargetWeights[ i ] *= attenuation;
                                m_vectorAccumulatedWeight[ i ] *= attenuation;
                            }
                        }
                    }

                    // For morph targets of Average type, we average the actions' scales
                    // according to the attenuatedWeight.  The first action assigns 100% of its
                    // scale, and subsequent actions do a weighted average of their scale with
                    // the accumulated scale.  The math works out.  By induction, you can reason
                    // that the result will weight all the scales in proportion to their given weights.
                    //
                    // The influence of any of the averaged morph targets is,
                    //
                    //    Scale * rampValue * ( attenuatedWeight / sumOfAttentuatedWeights )
                    //
                    // The units of this expression are scaleUnits * rampUnits, which matches the units
                    // for the other composition modes.  The term ( attenuatedWeight / sumOfAttentuatedWeights ),
                    // is a ratio that doesn't have any units.
                    float rampedScale = scale * rampValue;
                    if (m_vectorAccumulatedWeight[ i ] == 0.0f) {
                        morphTargetWeights[ i ] = rampedScale;
                    } else {
                        float factor = attenuatedWeight / (m_vectorAccumulatedWeight[ i ] + attenuatedWeight);
                        morphTargetWeights[ i ] = morphTargetWeights[ i ] * (1.0f - factor) + rampedScale * factor;
                    }
                    m_vectorAccumulatedWeight[ i ] += attenuatedWeight;
                    break;
                }
                default: {
                    assert(!"Unexpected");
                    break;
                }
            }
            return;
        }
    }
}
