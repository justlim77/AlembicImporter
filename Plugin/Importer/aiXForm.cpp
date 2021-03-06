#include "pch.h"
#include "AlembicImporter.h"
#include "aiLogger.h"
#include "aiContext.h"
#include "aiObject.h"
#include "aiSchema.h"
#include "aiXForm.h"


aiXFormSample::aiXFormSample(aiXForm *schema)
    : super(schema)
{
}

void aiXFormSample::updateConfig(const aiConfig &config, bool &topoChanged, bool &dataChanged)
{
    DebugLog("aiXFormSample::updateConfig()");
    
    topoChanged = false;
    dataChanged = (config.swapHandedness != m_config.swapHandedness);
    m_config = config;
}


inline abcV3 aiExtractTranslation(const AbcGeom::XformSample &sample, bool swapHandedness)
{
    abcV3 ret;

    size_t n = sample.getNumOps();
    size_t n_trans_op = 0;
    for (size_t i = 0; i < n; ++i) {
        const auto &op = sample.getOp(i);
        if (op.getType() == AbcGeom::kTranslateOperation) {
            if (++n_trans_op == 1) {
                ret = op.getVector();
            }
        }
        else if (op.getType() == AbcGeom::kMatrixOperation) {
            n_trans_op = 2;
        }
    }

    if (n_trans_op != 1) {
        ret = sample.getTranslation();
    }

    if (swapHandedness)
    {
        ret.x *= -1.0f;
    }
    return ret;
}

// return rotation in quaternion
inline abcV4 aiExtractRotation(const AbcGeom::XformSample &sample, bool swapHandedness)
{
    // XformSample は内部的にトランスフォームを行列で保持しており、getAxis(), getScale() などはその行列から値を復元しようとする。
    // このため回転とスケールが同時にかかっている場合返ってくる値は正確ではなくなってしまう。
    // これをなんとかするため、保持されている XformOp を見て kRotateOperation が 1 個だけあった場合はその axis angle を返し、
    // それ以外の場合 getAxis() に fallback する。

    const float Deg2Rad = (aiPI / 180.0f);

    abcV3 axis;
    float angle;

    size_t n = sample.getNumOps();
    size_t n_rot_op = 0;
    for (size_t i = 0; i < n; ++i) {
        const auto &op = sample.getOp(i);
        if (op.getType() == AbcGeom::kRotateOperation) {
            if (++n_rot_op == 1) {
                axis = op.getAxis();
                angle = (float)op.getAngle()* Deg2Rad;
            }
        }
        else if (op.getType() == AbcGeom::kMatrixOperation) {
            n_rot_op = 2;
        }
    }

    if (n_rot_op != 1) {
        axis = sample.getAxis();
        angle = (float)sample.getAngle()* Deg2Rad;
    }

    if (swapHandedness)
    {
        axis.x *= -1.0f;
        angle *= -1.0f;
    }

    float rs = std::sin(angle * 0.5f);
    float rc = std::cos(angle * 0.5f);
    return abcV4(axis.x*rs, axis.y*rs, axis.z*rs, rc);
}

inline abcV3 aiExtractScale(const AbcGeom::XformSample &sample)
{
    abcV3 ret;

    size_t n = sample.getNumOps();
    size_t n_scale_op = 0;
    for (size_t i = 0; i < n; ++i) {
        const auto &op = sample.getOp(i);
        if (op.getType() == AbcGeom::kScaleOperation) {
            if (++n_scale_op == 1) {
                ret = op.getVector();
            }
        }
        else if (op.getType() == AbcGeom::kMatrixOperation) {
            n_scale_op = 2;
        }
    }

    if (n_scale_op != 1) {
        ret = sample.getScale();
    }
    return ret;
}

void aiXFormSample::getData(aiXFormData &outData) const
{
    DebugLog("aiXFormSample::getData()");

    abcV3 trans = aiExtractTranslation(m_sample, m_config.swapHandedness);
    abcV4 rot = aiExtractRotation(m_sample, m_config.swapHandedness);
    abcV3 scale = aiExtractScale(m_sample);

    outData.inherits = m_sample.getInheritsXforms();
    outData.translation = trans;
    outData.rotation = rot;
    outData.scale = scale;
}



aiXForm::aiXForm(aiObject *obj)
    : super(obj)
{
}

aiXForm::Sample* aiXForm::newSample()
{
    Sample *sample = getSample();
    
    if (!sample)
    {
        sample = new Sample(this);
    }
    
    return sample;
}

aiXForm::Sample* aiXForm::readSample(const abcSampleSelector& ss, bool &topologyChanged)
{
    DebugLog("aiXForm::readSample(t=%f)", time);
    
    Sample *ret = newSample();

    m_schema.get(ret->m_sample, ss);

    topologyChanged = false;

    return ret;
}

