#include "ThinlensCamera.hpp"
#include <cmath>

void ThinlensCamera::precompute()
{
    float planeArea = (2.0f/_planeDist)*(2.0f*_ratio/_planeDist);
    _invPlaneArea = 1.0f/planeArea;
}

float ThinlensCamera::evalApertureThroughput(Vec3f planePos, Vec2f aperturePos) const
{
    float aperture = (*_aperture)[aperturePos].x();

    if (_catEye > 0.0f) {
        aperturePos = (aperturePos*2.0f - 1.0f)*_apertureSize;
        Vec3f lensPos = Vec3f(aperturePos.x(), aperturePos.y(), 0.0f);
        Vec3f localDir = (planePos - lensPos).normalized();
        Vec2f diaphragmPos = lensPos.xy() - _catEye*_planeDist*localDir.xy()/localDir.z();
        if (diaphragmPos.lengthSq() > sqr(_apertureSize))
            return 0.0f;
    }
    return aperture/_aperture->maximum().x();
}


bool ThinlensCamera::sampleDirection(PathSampleGenerator &sampler, const PositionSample &point, Vec2u pixel,
        DirectionSample &sample) const
{
    float pdf;
    Vec2f pixelUv = _filter.sample(sampler.next2D(), pdf);
    Vec3f planePos = Vec3f(
        -1.0f  + (float(pixel.x()) + pixelUv.x())*2.0f*_pixelSize.x(),
        _ratio - (float(pixel.y()) + pixelUv.y())*2.0f*_pixelSize.x(),
        _planeDist
    );
    planePos *= _focusDist/planePos.z();

    Vec3f lensPos = _invTransform*point.p;
    Vec3f localD = (planePos - lensPos).normalized();

    if (_catEye > 0.0f) {
        Vec2f diaphragmPos = lensPos.xy() - _catEye*_planeDist*localD.xy()/localD.z();
        if (diaphragmPos.lengthSq() > sqr(_apertureSize))
            return false;
    }

    sample.d =  _transform.transformVector(localD);
    sample.weight = Vec3f(1.0f);
    sample.pdf = _invPlaneArea/cube(localD.z());

    return true;
}


bool ThinlensCamera::evalDirection(PathSampleGenerator &/*sampler*/, const PositionSample &point,
        const DirectionSample &direction, Vec3f &weight, Vec2f &pixel) const
{
    Vec3f localLensPos = _invTransform*point.p;
    Vec3f localDir = _invTransform.transformVector(direction.d);
    if (localDir.z() <= 0.0f)
        return false;

    Vec3f planePos = localDir*(_focusDist/localDir.z()) + localLensPos;
    planePos *= _planeDist/planePos.z();

    if (_catEye > 0.0f) {
        Vec2f diaphragmPos = localLensPos.xy() - _catEye*_planeDist*localDir.xy()/localDir.z();
        if (diaphragmPos.lengthSq() > sqr(_apertureSize))
            return false;
    }

    pixel.x() = (planePos.x() + 1.0f)/(2.0f*_pixelSize.x());
    pixel.y() = (_ratio - planePos.y())/(2.0f*_pixelSize.x());
    if (pixel.x() < -_filter.width() || pixel.y() < -_filter.width() ||
        pixel.x() >= _res.x() || pixel.y() >= _res.y())
        return false;

    weight = Vec3f(sqr(_planeDist)/(4.0f*_pixelSize.x()*_pixelSize.x()*cube(localDir.z()/localDir.length())));
    return true;
}

float ThinlensCamera::directionPdf(const PositionSample &point, const DirectionSample &direction) const
{
    Vec3f localLensPos = _invTransform*point.p;
    Vec3f localDir = _invTransform.transformVector(direction.d);
    if (localDir.z() <= 0.0f)
        return false;

    Vec3f planePos = localDir*(_focusDist/localDir.z()) + localLensPos;
    planePos *= _planeDist/planePos.z();

    if (_catEye > 0.0f) {
        Vec2f diaphragmPos = localLensPos.xy() - _catEye*_planeDist*localDir.xy()/localDir.z();
        if (diaphragmPos.lengthSq() > sqr(_apertureSize))
            return false;
    }

    float u = (planePos.x() + 1.0f)*0.5f;
    float v = (1.0f - planePos.y()/_ratio)*0.5f;
    if (u < 0.0f || v < 0.0f || u > 1.0f || v > 1.0f)
        return 0.0f;

    return  _invPlaneArea/cube(localDir.z()/localDir.length());
}
