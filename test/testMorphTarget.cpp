#include "TestPrologue.h"
#include <cal3d/vector.h>
#include <cal3d/coremorphtarget.h>
#include <cal3d/streamops.h>

TEST(morph_target_applyZupToYup) {
    CalCoreMorphTarget::VertexOffsetArray vertexOffsets;
    VertexOffset mv;
    mv.vertexId = 0;
    mv.position = CalVector4(0, 1, 2, 0);
    mv.normal = CalVector4(0, -1, -2, 0);
    vertexOffsets.push_back(mv);
    CalCoreMorphTargetPtr morphTarget(new CalCoreMorphTarget("m", 1, vertexOffsets));

    morphTarget->applyZupToYup();
    
    const CalCoreMorphTarget::VertexOffsetArray& voses = morphTarget->vertexOffsets;
    const VertexOffset& vos = voses[0];

    CalVector expectingMvPos = mv.position.asCalVector();
    CalVector expectingMvNormal = mv.normal.asCalVector();

    cal3d::applyZupToYup(expectingMvPos);
    cal3d::applyZupToYup(expectingMvNormal);
    
    CHECK_EQUAL(vos.vertexId, static_cast<size_t>(0));
    CHECK_EQUAL(expectingMvPos, (vos.position.asCalVector()));
    CHECK_EQUAL(expectingMvNormal, (vos.normal.asCalVector()));

}

TEST(morph_target_applyCoordinateTransform) {
    // Note that cal3d quaternions are "left-handed", so that the following positive rotation is, in fact, the clockwise
    // rotation needed to convert z-up coordinates to y-up.
    CalQuaternion zUpToYUp(0.70710678f, 0.0f, 0.0f, 0.70710678f);
    CalCoreMorphTarget::VertexOffsetArray vertexOffsets;
    VertexOffset mv;
    mv.vertexId = 0;
    mv.position = CalVector4(0, 1, 2, 0);
    mv.normal = CalVector4(0, -1, -2, 0);
    vertexOffsets.push_back(mv);
    CalCoreMorphTargetPtr morphTarget(new CalCoreMorphTarget("m", 1, vertexOffsets));

    morphTarget->applyZupToYup();

    const CalCoreMorphTarget::VertexOffsetArray& voses = morphTarget->vertexOffsets;
    const VertexOffset& vos = voses[0];

    CalVector expectingMvPos = mv.position.asCalVector();
    CalVector expectingMvNormal = mv.normal.asCalVector();

    cal3d::applyZupToYup(expectingMvPos);
    cal3d::applyZupToYup(expectingMvNormal);

    CHECK_EQUAL(vos.vertexId, static_cast<size_t>(0));
    CHECK_EQUAL(expectingMvPos, (vos.position.asCalVector()));
    CHECK_EQUAL(expectingMvNormal, (vos.normal.asCalVector()));

}
