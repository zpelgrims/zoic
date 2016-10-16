#include "realistic.h"

#include <nori/common.h>
#include <nori/camera.h>
#include <nori/rfilter.h>
#include <nori/warp.h>
#include <nori/frame.h>
#include <filesystem/resolver.h>

#include <fstream>
#include <sstream>

#include <Geometry>

NORI_NAMESPACE_BEGIN

RealisticCamera::RealisticCamera(const PropertyList &propList) {

    m_filename = propList.getString("filename");

    // Image width and height in pixels
    m_outputSize.x() = propList.getInteger("width", 1280);
    m_outputSize.y() = propList.getInteger("height", 720);
    m_aspect = m_outputSize.x() / (float) m_outputSize.y();
    m_invOutputSize = m_outputSize.cast<float>().cwiseInverse();

    // Film width and height in mm
    m_filmSize.x() = propList.getInteger("filmWidth", 36);
    m_filmSize.y() = propList.getInteger("filmHeight", 24);

    // camera-to-world transformation
    m_cameraToWorld = propList.getTransform("toWorld", Transform());

    // Near and far clipping planes in world-space units
    m_nearClip = propList.getFloat("nearClip", 1e-4f);
    m_farClip = propList.getFloat("farClip", 1e4f);

    // World-unit to mm scale factor
    m_scale = propList.getFloat("scale", 1000.f);

    // Focus distance in world-space units
    m_focusDistance = propList.getFloat("focusDistance", 0.f);

    // Number of aperture blades
    m_blades = propList.getInteger("blades", 0);

    // Exposure correction
    m_exposureMultiplier = std::pow(2.f, propList.getFloat("exposure", 0.f));


    // Load lens definition
    filesystem::path filename = getFileResolver()->resolve(m_filename);
    m_elements = loadLensDefinition(filename);
    if (m_elements.empty()) {
        throw NoriException("Empty lens definition file '%s'", m_filename);
    }
    m_elements.back().thickness = 50.f;
    updateElementZ();

    // Compute focal length & f-stop
    m_focalLength = computeFocalLength();
    m_fStop = m_focalLength / (2.f * m_elements.front().aperture);

    // Adjust film size to given image size
    float filmAspect = m_filmSize.x() / m_filmSize.y();
    if (m_aspect > filmAspect) {
        m_filmSize = Vector2f(m_filmSize.x(), m_filmSize.x() / m_aspect);
    } else {
        m_filmSize = Vector2f(m_filmSize.y() * m_aspect, m_filmSize.y());
    }
    m_diagonal = m_filmSize.norm();

    // Set focus distance
    setFocusDistance(m_focusDistance);

    // Set f-stop
    if (propList.has("fstop")) {
        setFStop(propList.getFloat("fstop"));
    }
    //setFStop(m_fStop);

    // Compute exit pupil lookup table
    int NumSamples = 64;
    for (int i = 0; i < NumSamples; ++i) {
        float r0 = float(i) / NumSamples * 0.5f * m_diagonal;
        float r1 = float(i + 1) / NumSamples * 0.5f * m_diagonal;
        auto bounds = computeExitPupilBounds(r0, r1);
        m_exitPupilBounds.emplace_back(bounds);
    }
}

void RealisticCamera::activate() {
    Camera::activate();
}

Color3f RealisticCamera::sampleRay(Ray3f &ray,
        const Point2f &samplePosition,
        const Point2f &apertureSample) const {

    // Point on film
    Point3f filmP(
        (samplePosition.x() * m_invOutputSize.x() - 0.5f) * m_filmSize.x(),
        (0.5f - samplePosition.y() * m_invOutputSize.y()) * m_filmSize.y(),
        0.f
    );

    // Sample point on exit pupil
    Point3f rearP;
    float pupilArea;
    sampleExitPupil(filmP, apertureSample, rearP, pupilArea);

    // Trace ray through lens system
    Ray3f rayFilm(filmP, (rearP - filmP).normalized());
    if (!traceFromFilm(rayFilm, ray)) {
        return Color3f(0.f);
    }

    float cosTheta = Frame::cosTheta(ray.d);

    // Transform ray to world space
    // Move ray to origin z=0
    ray.o.z() -= getFrontElement().z;
    // Invert position/direction
    ray.o *= -1;
    ray.d *= -1;
    // Scale to world-units
    ray.o /= m_scale;
    // Transform to world space
    ray = m_cameraToWorld * ray;
    // Setup ray
    ray.d.normalize();
    ray.mint = m_nearClip;
    ray.maxt = m_farClip;
    ray.update();

    // Compute weight
    return (sqr(sqr(cosTheta)) * pupilArea) / (sqr(getRearElement().thickness)) * m_exposureMultiplier;
}

float RealisticCamera::getFarClip() const {
    return m_farClip;
}

/// Return a human-readable summary
std::string RealisticCamera::toString() const {
    return tfm::format(
        "RealisticCamera[\n"
        "  filename = %s,\n"
        "  cameraToWorld = %s,\n"
        "  outputSize = %s,\n"
        "  clip = [%f, %f],\n"
        "  filmSize = %s,\n"
        "  scale = %f,\n"
        "  focusDistance = %f,\n"
        "  rfilter = %s,\n"
        "  medium = %s\n"
        "]",
        m_filename,
        indent(m_cameraToWorld.toString(), 18),
        m_outputSize.toString(),
        m_nearClip,
        m_farClip,
        m_filmSize.toString(),
        m_scale,
        m_focusDistance,
        indent(m_rfilter->toString()),
        m_medium ? indent(m_medium->toString()) : std::string("null")
    );
}

/// Compute aperture size based on given fStop.
/// Note: This is approximate only as we assume aperture of front element to
/// be proportional to the aperture of the stop element.
void RealisticCamera::setFStop(float fStop) {
    m_fStop = std::max(m_focalLength / (2.f * getFrontElement().aperture), fStop);
    float frontAperture = m_focalLength / (2.f * m_fStop);
    getApertureElement().aperture = m_apertureRadiusOpen * frontAperture / getFrontElement().aperture;
}

void RealisticCamera::setFocusDistance(float focusDistance) {
    if (focusDistance <= -0.f) {
        // Focus at infinity
        getRearElement().thickness = computeFocalPlane() - getRearElement().z;
        updateElementZ();
    } else {
        getRearElement().thickness = computeFocus(focusDistance * m_scale);
        updateElementZ();
        cout << tfm::format("current focus distance: %f", computeFocusDistance()) << endl;
    }
    m_focusDistance = focusDistance;
}

/// Computes thick lens parameters.
bool RealisticCamera::computeThickLens(float P[2], float F[2]) const {

    auto computePrincipalPlaneAndFocalPoint = [] (const Ray3f &inRay, const Ray3f &outRay, float &P, float &F) {
        float Ft = -outRay.o.x() / outRay.d.x();
        F = outRay(Ft).z();
        float Pt = (inRay.o.x() - outRay.o.x()) / outRay.d.x();
        P = outRay(Pt).z();
    };

    // parallel ray from film
    Ray3f filmRay = Ray3f(Vector3f(1.f, 0.f, 0.f), Vector3f(0.f, 0.f, -1.f));
    Ray3f sceneRay;
    if (!traceFromFilm<true, true>(filmRay, sceneRay, [] (const Ray3f &) {} )) {
        return false;
    }
    computePrincipalPlaneAndFocalPoint(filmRay, sceneRay, P[0], F[0]);

    // parallel ray from scene
    sceneRay = Ray3f(Vector3f(1.f, 0.f, getFrontElement().z * 2.f), Vector3f(0.f, 0.f, 1.f));
    if (!traceFromScene<true, true>(sceneRay, filmRay, [] (const Ray3f &) {} )) {
        return false;
    }
    computePrincipalPlaneAndFocalPoint(sceneRay, filmRay, P[1], F[1]);

    return true;
}

/// Computes the focal length.
float RealisticCamera::computeFocalLength() const {
    float P[2], F[2];
    if (!computeThickLens(P, F)) {
        throw NoriException("Unable to compute focal length!");
    }
    return F[1] - P[1];
}

float RealisticCamera::computeFocalPlane() const {
    float P[2], F[2];
    if (!computeThickLens(P, F)) {
        throw NoriException("Unable to compute focal length!");
    }
    return F[1];
}

float RealisticCamera::computeFocus(float focusDistance) const {
    Vector3f focusP(0.f, 0.f, getFrontElement().z - focusDistance);
    Vector3f frontP(getFrontElement().aperture * 0.01f, 0.f, getFrontElement().z);
    Ray3f sceneRay(focusP, (frontP - focusP).normalized());
    Ray3f filmRay;
    if (!traceFromScene(sceneRay, filmRay)) {
        return 0.f;
        throw NoriException("Unable to compute focus!");
    }
    float t = -filmRay.o.x() / filmRay.d.x();
    float z = filmRay(t).z();
    float filmDistance = z - getRearElement().z;
    cout << tfm::format("focused to %f mm -> film distance from %f to %f", focusDistance, getRearElement().thickness, filmDistance) << endl;
    return filmDistance;
}

float RealisticCamera::computeFocusDistance() const {
    Vector3f filmP(0.f);
    Vector3f rearP(getRearElement().aperture * 0.01f, 0.f, getRearElement().z);
    Ray3f filmRay(filmP, (rearP - filmP).normalized());
    Ray3f sceneRay;
    if (!traceFromFilm(filmRay, sceneRay)) {
        return 0.f;
        throw NoriException("Unable to compute focus!");
    }
    float t = -sceneRay.o.x() / sceneRay.d.x();
    float z = sceneRay(t).z();
    float focusDistance = getFrontElement().z - z;
    return focusDistance;
}

BoundingBox2f RealisticCamera::computeExitPupilBounds(float r0, float r1) const {
    BoundingBox2f bounds;

    const int FilmSamples = 16;
    const int RearSamples = 16;

    float rearZ = getRearElement().z;
    float rearAperture = getRearElement().aperture;

    int numHit = 0;
    int numTraced = 0;
    for (int i = 0; i <= FilmSamples; ++i) {
        Vector3f filmP(nori::lerp(float(i) / FilmSamples, r0, r1), 0.f, 0.f);
        for (int x = -RearSamples; x <= RearSamples; ++x) {
            for (int y = -RearSamples; y <= RearSamples; ++y) {
                Vector3f rearP(x * (rearAperture / RearSamples), y * (rearAperture / RearSamples), rearZ);
                Ray3f sceneRay;
                if (traceFromFilm<false, true>(Ray3f(filmP, (rearP - filmP).normalized()), sceneRay, [] (const Ray3f &) {} )) {
                    bounds.expandBy(Vector2f(rearP.x(), rearP.y()));
                    ++numHit;
                }
                ++numTraced;
            }
        }
    }

    // Expand due to sampling error and clip to actual aperture
    bounds.expandBy(rearAperture / RearSamples);
    bounds.clip(BoundingBox2f(-rearAperture, rearAperture));

    return bounds;
}

void RealisticCamera::sampleExitPupil(const Point3f &filmP, const Point2f &apertureSample,
                                      Point3f &rearP, float &area) const {
    // Find pupil bounds for given distance to optical axis and sample a point
    float r = std::sqrt(sqr(filmP.x()) + sqr(filmP.y()));
    int index = std::floor(r / (0.5f * m_diagonal) * m_exitPupilBounds.size());
    index = std::min(index, int(m_exitPupilBounds.size()) - 1);
    const auto &bounds = m_exitPupilBounds[index];
    Point2f p = bounds.min + apertureSample.cwiseProduct(bounds.max - bounds.min);
    area = bounds.getVolume();

    // Transform pupil bounds to align with position on film
    float sinTheta = (r == 0.f) ? 0.f : filmP.y() / r;
    float cosTheta = (r == 0.f) ? 1.f : filmP.x() / r;
    rearP = Point3f(
        cosTheta * p.x() - sinTheta * p.y(),
        sinTheta * p.x() + cosTheta * p.y(),
        getRearElement().z
    );
}

template<bool SkipAperture, bool SimpleAperture, typename EmitRay>
bool RealisticCamera::traceFromFilm(const Ray3f &inRay, Ray3f &outRay, EmitRay emitRay) const {
    Ray3f ray(inRay);
    float z = 0.f;
    for (int i = m_elements.size() - 1; i >= 0; --i) {
        const auto &element = m_elements[i];
        bool isAperture = element.radius == 0.f;
        z -= element.thickness;

        // Intersect aperture plane or spherical lens element
        float t;
        Normal3f n;
        if (isAperture) {
            t = (z - ray.o.z()) / ray.d.z();
        } else {
            if (!intersectLensElement(element.radius, z + element.radius, ray, t, n)) {
                return false;
            }
        }

        emitRay(Ray3f(ray, 0.f, t));

        // Check if we hit aperture
        Vector3f p = ray(t);
        if (!SkipAperture || i != int(m_apertureIndex)) {
            if (SimpleAperture) {
                if (sqr(p.x()) + sqr(p.y()) > sqr(element.aperture)) {
                    return false;
                }
            } else {
                if (hitAperture(Point2f(p.x(), p.y()))) {
                    return false;
                }
            }
        }

        // Refract ray at lens element
        if (!isAperture) {
            float etaI = (element.eta != 0.f) ? element.eta : 1.f;
            float etaT = (i > 0 && m_elements[i - 1].eta != 0.f) ? m_elements[i - 1].eta : 1.f;
            Vector3f wo;
            if (!refract(n, etaI / etaT, ray.d, ray.d)) {
                return false;
            }
            ray.o = p;
        }
    }

    emitRay(Ray3f(ray, 0.f, Infinity));

    outRay = ray;
    return true;
}

bool RealisticCamera::traceFromFilm(const Ray3f &rayIn, Ray3f &rayOut) const {
    return traceFromFilm<false, false>(rayIn, rayOut, [] (const Ray3f &) {} );
}

bool RealisticCamera::traceFromFilm(const Ray3f &inRay, std::vector<Ray3f> &outRays) const {
    Ray3f outRay;
    return traceFromFilm<false, false>(inRay, outRay, [&outRays] (const Ray3f &ray) { outRays.emplace_back(ray); });
}

template<bool SkipAperture, bool SimpleAperture, typename EmitRay>
bool RealisticCamera::traceFromScene(const Ray3f &inRay, Ray3f &outRay, EmitRay emitRay) const {
    Ray3f ray(inRay);
    float z = getFrontElement().z;
    for (size_t i = 0; i < m_elements.size(); ++i) {
        const auto &element = m_elements[i];
        bool isAperture = element.radius == 0.f;

        // Intersect aperture plane or spherical lens element
        float t;
        Normal3f n;
        if (isAperture) {
            t = (z - ray.o.z()) / ray.d.z();
        } else {
            if (!intersectLensElement(element.radius, z + element.radius, ray, t, n)) {
                return false;
            }
        }

        emitRay(Ray3f(ray, 0.f, t));

        // Check if we hit aperture
        Vector3f p = ray(t);
        if (!SkipAperture || i != m_apertureIndex) {
            if (SimpleAperture) {
                if (sqr(p.x()) + sqr(p.y()) > sqr(element.aperture)) {
                    return false;
                }
            } else {
                if (hitAperture(Point2f(p.x(), p.y()))) {
                    return false;
                }
            }
        }

        // Refract ray at lens element
        if (!isAperture) {
            float etaI = (i == 0 || m_elements[i - 1].eta == 0.f) ? 1.f : m_elements[i - 1].eta;
            float etaT = (element.eta != 0.f) ? element.eta : 1.f;
            Vector3f wo;
            if (!refract(n, etaI / etaT, ray.d, ray.d)) {
                return false;
            }
            ray.o = p;
        }

        z += element.thickness;
    }

    emitRay(Ray3f(ray, 0.f, Infinity));

    outRay = ray;
    return true;
}


bool RealisticCamera::traceFromScene(const Ray3f &rayIn, Ray3f &rayOut) const {
    return traceFromScene<false, false>(rayIn, rayOut, [] (const Ray3f &) {} );
}

bool RealisticCamera::traceFromScene(const Ray3f &inRay, std::vector<Ray3f> &outRays) const {
    Ray3f outRay;
    return traceFromScene<false, false>(inRay, outRay, [&outRays] (const Ray3f &ray) { outRays.emplace_back(ray); });
}

bool RealisticCamera::hitAperture(const Point2f &p) const {
    float radius = getApertureElement().aperture;
    if (m_blades < 3) {
        return sqr(p.x()) + sqr(p.y()) > sqr(radius);
    } else {
        float thetaBladeHalf = M_PI / m_blades;
        float r = p.norm();
        float theta = std::atan2(p.y(), p.x()) + TWO_PI;
        theta = std::fmod(theta + thetaBladeHalf, 2.f * thetaBladeHalf) - thetaBladeHalf;
        Point2f rp(r * std::cos(theta), r * std::sin(theta));
        //cout << tfm::format("p=%f/%f theta=%f r=%f rp=%f/%f", p.x(), p.y(), theta, r, rp.x(), rp.y()) << endl;
        return rp.x() > radius;

        return false;
    }
}

bool RealisticCamera::intersectLensElement(float radius, float center, const Ray3f &ray, float &t, Normal3f &n) const {
    Point3f o = ray.o - Vector3f(0.f, 0.f, center);
    float A = ray.d.dot(ray.d);
    float B = 2.f * ray.d.dot(o);
    float C = o.dot(o) - sqr(radius);
    float t0, t1;
    if (!solveQuadratic(A, B, C, t0, t1)) {
        return false;
    }

    // Choose intersection distance based on ray direction and lens curvature
    t = (ray.d.z() > 0.f) ^ (radius < 0.f) ? std::min(t0, t1) : std::max(t0, t1);
    if (t < 0.f) {
        return false;
    }

    // Compute normal (flip to make front facing if necessary)
    n = (o + t * ray.d).normalized();
    if (n.dot(ray.d) > 0.f) {
        n = -n;
    }
    return true;
}

bool RealisticCamera::refract(const Vector3f &n, float eta, const Vector3f &wi, Vector3f &wo) const {
    float cosThetaWi = n.dot(wi);
    float k = 1.f - sqr(eta) * (1.f - sqr(cosThetaWi));
    if (k < 0.f) {
        return false;
    }
    wo = eta * wi - (eta * cosThetaWi + std::sqrt(k)) * n;
    return true;
}

std::vector<RealisticCamera::LensElement> RealisticCamera::loadLensDefinition(const filesystem::path &filename) {
    std::vector<LensElement> elements;

    std::ifstream is(filename.str());
    if (is.fail()) {
        throw NoriException("Unable to open lens definition file \"%s\"!", filename);
    }

    std::string line;
    float z = 0.f;
    while (std::getline(is, line)) {
        if (line.empty() || line.front() == '#') {
            continue;
        }
        std::istringstream iss(line);
        LensElement element;
        element.z = z;
        iss >> element.radius >> element.thickness >> element.eta >> element.aperture;
        element.aperture *= 0.5f; // diameter -> radius
        if (element.radius == 0.f) {
            m_apertureIndex = elements.size();
            m_apertureRadiusOpen = element.aperture;
        }
        elements.emplace_back(element);
        z += element.thickness;
    }

    return elements;
}

/// Update the z coordinates of all lens elements.
void RealisticCamera::updateElementZ() {
    float z = 0.f;
    for (int i = m_elements.size() - 1; i >= 0; --i) {
        auto &element = m_elements[i];
        z -= element.thickness;
        element.z = z;
    }
}

NORI_REGISTER_CLASS(RealisticCamera, "realistic");
NORI_NAMESPACE_END
