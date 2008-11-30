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

 /*****************************************************************************/
/** Constructs the submesh instance.
  *
  * This function is the default constructor of the submesh instance.
  *****************************************************************************/

CalSubmesh::CalSubmesh()
  : m_pCoreSubmesh(0)
{
}

 /*****************************************************************************/
/** Creates the submesh instance.
  *
  * This function creates the submesh instance based on a core submesh.
  *
  * @param pCoreSubmesh A pointer to the core submesh on which this submesh
  *                     instance should be based on.
  *
  * @return One of the following values:
  *         \li \b true if successful
  *         \li \b false if an error happend
  *****************************************************************************/

bool CalSubmesh::create(CalCoreSubmesh *pCoreSubmesh)
{
  if(pCoreSubmesh == 0)
  {
    CalError::setLastError(CalError::INVALID_HANDLE, __FILE__, __LINE__);
    return false;
  }

  m_pCoreSubmesh = pCoreSubmesh;

  // reserve memory for the face vector
  m_vectorFace.reserve(m_pCoreSubmesh->getFaceCount());
  m_vectorFace.resize(m_pCoreSubmesh->getFaceCount());

  // set the initial lod level
  setLodLevel(1.0f);

  //Setting the morph target weights
  m_vectorMorphTargetWeight.reserve(m_pCoreSubmesh->getCoreSubMorphTargetCount());
  m_vectorMorphTargetWeight.resize(m_pCoreSubmesh->getCoreSubMorphTargetCount());
  m_vectorAccumulatedWeight.reserve(m_pCoreSubmesh->getCoreSubMorphTargetCount());
  m_vectorAccumulatedWeight.resize(m_pCoreSubmesh->getCoreSubMorphTargetCount());
  m_vectorReplacementAttenuation.reserve(m_pCoreSubmesh->getCoreSubMorphTargetCount());
  m_vectorReplacementAttenuation.resize(m_pCoreSubmesh->getCoreSubMorphTargetCount());  

  // Array is indexed by group, and there can't be more groups than there are morph targets.
  m_vectorSubMorphTargetGroupAttenuator.reserve(m_pCoreSubmesh->getCoreSubMorphTargetCount());
  m_vectorSubMorphTargetGroupAttenuator.resize(m_pCoreSubmesh->getCoreSubMorphTargetCount());
  m_vectorSubMorphTargetGroupAttenuation.reserve(m_pCoreSubmesh->getCoreSubMorphTargetCount());
  m_vectorSubMorphTargetGroupAttenuation.resize(m_pCoreSubmesh->getCoreSubMorphTargetCount());  

  int morphTargetId;
  for(morphTargetId = 0; morphTargetId<m_pCoreSubmesh->getCoreSubMorphTargetCount();++morphTargetId)
  {
    m_vectorSubMorphTargetGroupAttenuator[ morphTargetId ] = -1; // No attenuator by default.
    m_vectorSubMorphTargetGroupAttenuation[ morphTargetId ] = 0.0f; // No attenuation by default.
    m_vectorMorphTargetWeight[morphTargetId] = 0.0f;
    m_vectorAccumulatedWeight[morphTargetId] = 0.0f;
    m_vectorReplacementAttenuation[morphTargetId] = ReplacementAttenuationNull;
  }

  return true;
}

 /*****************************************************************************/
/** Provides access to the core submesh.
  *
  * This function returns the core submesh on which this submesh instance is
  * based on.
  *
  * @return One of the following values:
  *         \li a pointer to the core submesh
  *         \li \b 0 if an error happend
  *****************************************************************************/

CalCoreSubmesh *CalSubmesh::getCoreSubmesh()
{
  return m_pCoreSubmesh;
}

 /*****************************************************************************/
/** Returns the normal vector.
  *
  * This function returns the vector that contains all normals of the submesh
  * instance.
  *
  * @return A reference to the normal vector.
  *****************************************************************************/

std::vector<CalVector>& CalSubmesh::getVectorNormal()
{
  return m_vectorNormal;
}


 /*****************************************************************************/
/** Returns the vertex vector.
  *
  * This function returns the vector that contains all vertices of the submesh
  * instance.
  *
  * @return A reference to the vertex vector.
  *****************************************************************************/

std::vector<CalVector>& CalSubmesh::getVectorVertex()
{
  return m_vectorVertex;
}

 /*****************************************************************************/
/** Returns the number of vertices.
  *
  * This function returns the number of vertices in the submesh instance.
  *
  * @return The number of vertices.
  *****************************************************************************/

int CalSubmesh::getVertexCount()
{
  return m_vertexCount;
}

 /*****************************************************************************/
/** Sets the LOD level.
  *
  * This function sets the LOD level of the submesh instance.
  *
  * @param lodLevel The LOD level in the range [0.0, 1.0].
  *****************************************************************************/

void CalSubmesh::setLodLevel(float lodLevel)
{
  // clamp the lod level to [0.0, 1.0]
  if(lodLevel < 0.0f) lodLevel = 0.0f;
  if(lodLevel > 1.0f) lodLevel = 1.0f;

  // get the lod count of the core submesh
  int lodCount;
  lodCount = m_pCoreSubmesh->getLodCount();

  // calculate the target lod count
  lodCount = (int)((1.0f - lodLevel) * lodCount);

  // calculate the new number of vertices
  m_vertexCount = m_pCoreSubmesh->getVertexCount() - lodCount;

  std::vector<CalCoreSubmesh::Face>& vectorFace = m_pCoreSubmesh->getVectorFace();
  std::vector<CalCoreSubmesh::Vertex>& vectorVertex = m_pCoreSubmesh->getVectorVertex();
  std::vector<CalCoreSubmesh::LodData>& lodData = m_pCoreSubmesh->getLodData();

  // calculate the new number of faces
  m_faceCount = vectorFace.size();

  for(int vertexId = vectorVertex.size() - 1; vertexId >= m_vertexCount; vertexId--)
  {
    m_faceCount -= lodData[vertexId].faceCollapseCount;
  }

  // fill the face vector with the collapsed vertex ids
  for(int faceId = 0; faceId < m_faceCount; ++faceId)
  {
    for(int vertexId = 0; vertexId < 3; ++vertexId)
    {
      // get the vertex id
      CalIndex collapsedVertexId;
      collapsedVertexId = vectorFace[faceId].vertexId[vertexId];

      // collapse the vertex id until it fits into the current lod level
      while(collapsedVertexId >= m_vertexCount)
      {
          collapsedVertexId = lodData[collapsedVertexId].collapseId;
      }

      // store the collapse vertex id in the submesh face vector
      m_vectorFace[faceId].vertexId[vertexId] = collapsedVertexId;
    }
  }
}

 /*****************************************************************************/
/** Sets weight of a morph target with the given id.
  *
  * @param blendId The morph target id.
  * @param weight The weight to be set.
  *****************************************************************************/

void CalSubmesh::setMorphTargetWeight(int blendId,float weight)
{
  m_vectorMorphTargetWeight[blendId] = weight;
}

 /*****************************************************************************/
/** Gets weight of a morph target with the given id.
  *
  * @param blendId The morph target id.
  * @return The weight of the morph target.
  *****************************************************************************/

float CalSubmesh::getMorphTargetWeight(int blendId)
{
  return m_vectorMorphTargetWeight[blendId];
}

void
CalSubmesh::getMorphIdAndWeightArray( MorphIdAndWeight * arrayResult, 
                                     unsigned int * numMiawsResult, 
                                     unsigned int maxMiaws )
{
  if( maxMiaws == 0 ) {
    * numMiawsResult = 0;
    return;
  }
  unsigned int j = 0;
  unsigned int morphTargetCount = ( unsigned int ) getMorphTargetWeightCount();
  unsigned int i;
  for( i = 0; i < morphTargetCount; i++ ) {
    float weight = m_vectorMorphTargetWeight[ i ];
    if( weight != 0 ) {
      arrayResult[ j ].morphId_ = i;
      arrayResult[ j ].weight_ = weight;
      j++;
      if( j == maxMiaws ) break;
    }
  }
  * numMiawsResult = j;
}


 /*****************************************************************************/
/** Sets weight of a morph target with the given name
  *
  * @param morphName The morph target name.
  * @param weight The weight to be set.
  *****************************************************************************/

void CalSubmesh::setMorphTargetWeight(std::string const & morphName,float weight)
{
  // TODO: this is very inefficient. we should probably use a map instead
  for( size_t i = 0; i < m_vectorMorphTargetWeight.size(); i++ ) {
    boost::shared_ptr<CalCoreSubMorphTarget> target = m_pCoreSubmesh->getCoreSubMorphTarget(i);
    if( target->name() == morphName ) {
      m_vectorMorphTargetWeight[i] = weight;
      return;
    }
  }
}


 /*****************************************************************************/
/** Clears the scale of each morph target.
  *
  * Call this in preparation for calling blendMorphTargetScale() on the individual
  * morph targets.  As a side effect, this also clears the "weight" that
  * setMorphTargetWeight() sets.  The functions clear/blendMorphTargetScale()
  * and setMorphTargetWeight() both set the influence of the morph target, but
  * call it by different names (scale vs. weight) and have different composition
  * behavior.  Call one set of functions or the other.
  * 
  *****************************************************************************/
void 
CalSubmesh::clearMorphTargetScales()
{
  int size = m_vectorMorphTargetWeight.size();
  for( int i = 0; i < size; i++ ) {
    m_vectorMorphTargetWeight[i] = 0.0f;
    m_vectorAccumulatedWeight[ i ] = 0.0f;
    m_vectorReplacementAttenuation[ i ] = ReplacementAttenuationNull;
  }
}


void 
CalSubmesh::clearMorphTargetState( std::string const & morphName )
{
  // TODO: this is very inefficient. we should probably use a map instead
  for( size_t i = 0; i < m_vectorMorphTargetWeight.size(); i++ ) {
    boost::shared_ptr<CalCoreSubMorphTarget> target = m_pCoreSubmesh->getCoreSubMorphTarget(i);
    if( target->name() == morphName ) {
      m_vectorMorphTargetWeight[i] = 0.0f;
      m_vectorAccumulatedWeight[ i ] = 0.0f;
      m_vectorReplacementAttenuation[ i ] = ReplacementAttenuationNull;
    }
  }
}


void
CalSubmesh::setSubMorphTargetGroupAttenuatorArray( unsigned int len, int const * morphTargetIdArray )
{

  // Array is indexed by group, and there can't be more groups than there are morph targets.
  assert( len <= m_vectorSubMorphTargetGroupAttenuator.size() );
  unsigned int i;
  for( i = 0; i < len; i++ ) {
    m_vectorSubMorphTargetGroupAttenuator[ i ] = morphTargetIdArray[ i ];
  }
}

void
CalSubmesh::setSubMorphTargetGroupAttenuationArray( unsigned int len, float const * attenuationArray )
{

  // Array is indexed by group, and there can't be more groups than there are morph targets.
  assert( len <= m_vectorSubMorphTargetGroupAttenuation.size() );
  unsigned int i;
  for( i = 0; i < len; i++ ) {
    m_vectorSubMorphTargetGroupAttenuation[ i ] = attenuationArray[ i ];
  }
}

 /*****************************************************************************/
/** Sets weight of a morph target with the given name
  *
  * @param morphName The morph target name.
  * @param scale Scale from -inf to inf scales magnitude.
  * @param unrampedWeight The blending weight, not incorporating ramp value
  * @param rampValue Amount to attenuate weight when ramping in/out the animation.
  * @param replace If true, all blends except one Replace blend will have their weight 
  *  attenuated by 1 - rampValue of that Replace blend.
  *****************************************************************************/
void 
CalSubmesh::blendMorphTargetScale( std::string const & morphName, 
                                  float scale, 
                                  float unrampedWeight, 
                                  float rampValue,
                                  bool replace )
{
  int size = m_vectorMorphTargetWeight.size();
  int i;
  for( i = 0; i < size; i++ ) {
    boost::shared_ptr<CalCoreSubMorphTarget> target = m_pCoreSubmesh->getCoreSubMorphTarget( i );
    if( target->name() == morphName ) {
      CalMorphTargetType mtype = target->morphTargetType();
      switch( mtype ) {
      case CalMorphTargetTypeAdditive:
        {

          // Actions affecting the same morph target channel add their ramped scales
          // if the channel is Additive.  The unrampedWeight parameter is ignored
          // because the actions are not affecting each other so there is no need
          // to assign them a relative weight.
          m_vectorMorphTargetWeight[ i ] += scale * rampValue;
          break;
        }
      case CalMorphTargetTypeClamped:
        {

          // Like Additive, but clamped to 1.0.
          m_vectorMorphTargetWeight[ i ] += scale * rampValue;
          if( m_vectorMorphTargetWeight[ i ] > 1.0 ) {
            m_vectorMorphTargetWeight[ i ] = 1.0;
          }
          break;
        }
      case CalMorphTargetTypeExclusive:
      case CalMorphTargetTypeAverage:
        {

          unsigned int subMorphTargetGroupIndex = m_pCoreSubmesh->subMorphTargetGroupIndex( i );
          float attenuatedWeight = unrampedWeight * rampValue;

          // If morph target is not in a group, then the group exclusivity doesn't apply.
          if( subMorphTargetGroupIndex == 0xffffffff ) {

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
            if( mtype == CalMorphTargetTypeExclusive ) {
              if( m_vectorReplacementAttenuation[ i ] != ReplacementAttenuationNull ) {
                attenuatedWeight *= m_vectorReplacementAttenuation[ i ];
              } else {
                if( replace ) {
                  float attenuation = 1.0f - rampValue;
                  m_vectorReplacementAttenuation[ i ] = attenuation;
                  m_vectorMorphTargetWeight[ i ] *= attenuation;
                  m_vectorAccumulatedWeight[ i ] *= attenuation;
                }
              }
            }
          } else {
            int attenuator = m_vectorSubMorphTargetGroupAttenuator[ subMorphTargetGroupIndex ];
            if( attenuator != -1 ) { // i.e., if group is exclusive
              if( attenuator != i ) { // If I'm not the attenuator, then I get attenuated.
                attenuatedWeight *= m_vectorSubMorphTargetGroupAttenuation[ subMorphTargetGroupIndex ];
              } else {
                
                // For group attenuation, I don't need to attenuate the earlier accumulation
                // (m_vectorMorphTargetWeight, m_vectorAccumulatedWeight) because the group's
                // attenuation is applied throughout the entire iteration.  Also, I don't
                // need to test for the "replace" flag because it has already been taken into account
                // in the decision of who the group attenuator is.
                float attenuation = 1.0f - rampValue;
                m_vectorReplacementAttenuation[ i ] = attenuation;
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
          if( m_vectorAccumulatedWeight[ i ] == 0.0f ) {
            m_vectorMorphTargetWeight[ i ] = rampedScale;
          } else {
            float factor = attenuatedWeight / ( m_vectorAccumulatedWeight[ i ] + attenuatedWeight );
            m_vectorMorphTargetWeight[ i ] = m_vectorMorphTargetWeight[ i ] * ( 1.0f - factor ) + rampedScale * factor;
          }
          m_vectorAccumulatedWeight[ i ] += attenuatedWeight;
          break;
        }
      default:
        {
          assert( !"Unexpected" );
          break;
        }
      }
      return;
    }
  }
}


 /*****************************************************************************/
/** Gets weight of a morph target with the given name
  *
  * @param morphName The morph target name
  * @param weightOut fills in the weight on success
  * @return true on success, false otherwise
  *****************************************************************************/

bool CalSubmesh::getMorphTargetWeight(std::string const & morphName, float * weightOut)
{
  for( size_t i = 0; i < m_vectorMorphTargetWeight.size(); i++ ) {
    boost::shared_ptr<CalCoreSubMorphTarget> target = m_pCoreSubmesh->getCoreSubMorphTarget(i);
    if( target->name() == morphName ) {
      *weightOut = m_vectorMorphTargetWeight[i];
      return true;
    }
  }
  return false;
}

 /*****************************************************************************/
/** Gets weight of the base vertices.
  *
  * @return The weight of the base vertices.
  *****************************************************************************/

float CalSubmesh::getBaseWeight()
{
  float baseWeight = 1.0f;
  int morphTargetCount = getMorphTargetWeightCount();
  int morphTargetId;
  for(morphTargetId=0; morphTargetId < morphTargetCount;++morphTargetId)
  {
    baseWeight -= m_vectorMorphTargetWeight[morphTargetId];
  }
  return baseWeight;
}

 /*****************************************************************************/
/** Returns the morph target weight vector.
  *
  * This function returns the vector that contains all weights for
  * each morph target instance.
  *
  * @return A reference to the weight vector.
  *****************************************************************************/
std::vector<float>& CalSubmesh::getVectorMorphTargetWeight()
{
  return m_vectorMorphTargetWeight;
}

 /*****************************************************************************/
/** Returns the number of weights.
  *
  * This function returns the number of weights.
  *
  * @return The number of weights.
  *****************************************************************************/

int CalSubmesh::getMorphTargetWeightCount()
{
  return m_vectorMorphTargetWeight.size();
}

//****************************************************************************//
