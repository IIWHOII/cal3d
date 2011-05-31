//****************************************************************************//
// coresubmesh.cpp                                                            //
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

#include "cal3d/coresubmesh.h"
#include "cal3d/coresubmorphtarget.h"

CalCoreSubmesh::CalCoreSubmesh(int vertexCount, int textureCoordinateCount, int faceCount)
    : m_coreMaterialThreadId(0)
    , m_isStatic(false)
    , m_currentVertexId(0) {
    m_hasNonWhiteVertexColors = false;

    // reserve the space needed in all the vectors
    m_vertices.resize(vertexCount);
    m_vertexColors.resize(vertexCount);

    m_vectorvectorTextureCoordinate.resize(textureCoordinateCount);
    for (int textureCoordinateId = 0; textureCoordinateId < textureCoordinateCount; ++textureCoordinateId) {
        m_vectorvectorTextureCoordinate[textureCoordinateId].resize(vertexCount);
    }

    m_vectorFace.resize(faceCount);
}

void CalCoreSubmesh::setSubMorphTargetGroupIndexArray(unsigned int len, unsigned int const* indexArray) {
    m_vectorSubMorphTargetGroupIndex.reserve(len);
    m_vectorSubMorphTargetGroupIndex.resize(len);
    unsigned int i;
    for (i = 0; i < len; i++) {
        m_vectorSubMorphTargetGroupIndex[ i ] = indexArray[ i ];
    }
}

CAL3D_DEFINE_SIZE(CalCoreSubmesh::Face);
CAL3D_DEFINE_SIZE(CalCoreSubmesh::Influence);

size_t sizeInBytes(const CalCoreSubmesh::InfluenceSet& is) {
    return sizeof(is) + sizeInBytes(is.influences);
}

size_t CalCoreSubmesh::sizeInBytes() const {
    size_t r = sizeof(*this);
    r += ::sizeInBytes(m_vertices);
    r += ::sizeInBytes(m_vertexColors);
    r += ::sizeInBytes(m_vectorFace);
    r += ::sizeInBytes(m_vectorSubMorphTargetGroupIndex);
    r += ::sizeInBytes(m_staticInfluenceSet);
    r += ::sizeInBytes(m_influences);
    return r;
}

const std::vector<CalCoreSubmesh::Face>& CalCoreSubmesh::getVectorFace() const {
    return m_vectorFace;
}

void CalCoreSubmesh::setCoreMaterialThreadId(int coreMaterialThreadId) {
    m_coreMaterialThreadId = coreMaterialThreadId;
}

bool CalCoreSubmesh::setFace(int faceId, const Face& face) {
    if ((faceId < 0) || (faceId >= (int)m_vectorFace.size())) {
        return false;
    }

    m_vectorFace[faceId] = face;
    return true;
}

bool CalCoreSubmesh::setTextureCoordinate(int vertexId, int textureCoordinateId, const TextureCoordinate& textureCoordinate) {
    if ((textureCoordinateId < 0) || (textureCoordinateId >= (int)m_vectorvectorTextureCoordinate.size())) {
        return false;
    }
    if ((vertexId < 0) || (vertexId >= (int)m_vectorvectorTextureCoordinate[textureCoordinateId].size())) {
        return false;
    }

    m_vectorvectorTextureCoordinate[textureCoordinateId][vertexId] = textureCoordinate;

    return true;
}

void CalCoreSubmesh::addVertex(const Vertex& vertex, CalColor32 vertexColor, const std::vector<Influence>& inf_) {
    assert(m_currentVertexId < m_vertices.size());

    const int vertexId = m_currentVertexId++;
    if (vertexId == 0) {
        m_isStatic = true;
        m_staticInfluenceSet = inf_;
        m_boundingVolume.min = vertex.position.asCalVector();
        m_boundingVolume.max = vertex.position.asCalVector();
    } else if (m_isStatic) {
        m_isStatic = m_staticInfluenceSet == inf_;
    }

    if (vertexId) {
        m_boundingVolume.min.x = std::min(m_boundingVolume.min.x, vertex.position.x);
        m_boundingVolume.min.y = std::min(m_boundingVolume.min.y, vertex.position.y);
        m_boundingVolume.min.z = std::min(m_boundingVolume.min.z, vertex.position.z);

        m_boundingVolume.max.x = std::max(m_boundingVolume.max.x, vertex.position.x);
        m_boundingVolume.max.y = std::max(m_boundingVolume.max.y, vertex.position.y);
        m_boundingVolume.max.z = std::max(m_boundingVolume.max.z, vertex.position.z);
    }

    m_vertices[vertexId] = vertex;
    m_vertexColors[vertexId] = vertexColor;

    // Each vertex needs at least one influence.
    std::vector<Influence> inf(inf_);
    if (inf.empty()) {
        m_isStatic = false;
        Influence i;
        i.boneId = 0;
        i.weight = 0.0f;
        inf.push_back(i);
    }

    // Mark the last influence as the last one.  :)
    for (size_t i = 0; i + 1 < inf.size(); ++i) {
        inf[i].lastInfluenceForThisVertex = 0;
    }
    inf[inf.size() - 1].lastInfluenceForThisVertex = 1;

    m_influences.insert(m_influences.end(), inf.begin(), inf.end());
}

int CalCoreSubmesh::addCoreSubMorphTarget(boost::shared_ptr<CalCoreSubMorphTarget> pCoreSubMorphTarget) {
    int subMorphTargetId = m_vectorCoreSubMorphTarget.size();
    m_vectorCoreSubMorphTarget.push_back(pCoreSubMorphTarget);
    return subMorphTargetId;
}

int CalCoreSubmesh::getCoreSubMorphTargetCount() {
    return m_vectorCoreSubMorphTarget.size();
}

CalCoreSubmesh::CoreSubMorphTargetVector& CalCoreSubmesh::getVectorCoreSubMorphTarget() {
    return m_vectorCoreSubMorphTarget;
}

void CalCoreSubmesh::scale(float factor) {
    for (int vertexId = 0; vertexId < m_vertices.size(); vertexId++) {
        m_vertices[vertexId].position *= factor;
    }
}

bool CalCoreSubmesh::isStatic() const {
    return m_isStatic && m_vectorCoreSubMorphTarget.empty();
}

BoneTransform CalCoreSubmesh::getStaticTransform(const BoneTransform* bones) const {
    BoneTransform rm;

    std::set<Influence>::const_iterator current = m_staticInfluenceSet.influences.begin();
    while (current != m_staticInfluenceSet.influences.end()) {
        const BoneTransform& influence = bones[current->boneId];
        rm.rowx.x += current->weight * influence.rowx.x;
        rm.rowx.y += current->weight * influence.rowx.y;
        rm.rowx.z += current->weight * influence.rowx.z;
        rm.rowx.w += current->weight * influence.rowx.w;

        rm.rowy.x += current->weight * influence.rowy.x;
        rm.rowy.y += current->weight * influence.rowy.y;
        rm.rowy.z += current->weight * influence.rowy.z;
        rm.rowy.w += current->weight * influence.rowy.w;

        rm.rowz.x += current->weight * influence.rowz.x;
        rm.rowz.y += current->weight * influence.rowz.y;
        rm.rowz.z += current->weight * influence.rowz.z;
        rm.rowz.w += current->weight * influence.rowz.w;

        ++current;
    }

    return rm;
}
