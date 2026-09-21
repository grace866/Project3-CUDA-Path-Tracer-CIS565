#include "interactions.h"

#include "utilities.h"

#include <thrust/random.h>
#include <cmath>

__host__ __device__ glm::vec3 calculateRandomDirectionInHemisphere(
    glm::vec3 normal,
    thrust::default_random_engine &rng)
{
    thrust::uniform_real_distribution<float> u01(0, 1);

    float up = sqrt(u01(rng)); // cos(theta)
    float over = sqrt(1 - up * up); // sin(theta)
    float around = u01(rng) * TWO_PI;

    // Find a direction that is not the normal based off of whether or not the
    // normal's components are all equal to sqrt(1/3) or whether or not at
    // least one component is less than sqrt(1/3). Learned this trick from
    // Peter Kutz.

    glm::vec3 directionNotNormal;
    if (abs(normal.x) < SQRT_OF_ONE_THIRD)
    {
        directionNotNormal = glm::vec3(1, 0, 0);
    }
    else if (abs(normal.y) < SQRT_OF_ONE_THIRD)
    {
        directionNotNormal = glm::vec3(0, 1, 0);
    }
    else
    {
        directionNotNormal = glm::vec3(0, 0, 1);
    }

    // Use not-normal direction to generate two perpendicular directions
    glm::vec3 perpendicularDirection1 =
        glm::normalize(glm::cross(normal, directionNotNormal));
    glm::vec3 perpendicularDirection2 =
        glm::normalize(glm::cross(normal, perpendicularDirection1));

    return up * normal
        + cos(around) * over * perpendicularDirection1
        + sin(around) * over * perpendicularDirection2;
}

__host__ __device__ glm::vec3 calculateReflectedRayDirection(
    glm::vec3 normal,
    glm::vec3 wo) {

    glm::vec3 wi = glm::reflect(wo, normal);

    return wi;
}

__host__ __device__ glm::vec3 sphericalToCartesian(
    float theta,
    float phi
) {
    float x = sin(theta) * cos(phi);
    float y = cos(theta);
    float z = sin(theta) * sin(phi);

    return glm::vec3(x, y, z);
}

__host__ __device__ glm::vec3 schlickFresnel(
    glm::vec3 r0, 
    float radians) {
    float exponential = pow(1.0f - radians, 5.0f);
    return r0 + (1.0f - r0) * exponential;
}

__host__ __device__ float smithGGXMaskingShadowing(
    glm::vec3 wi,
    glm::vec3 wo,
    float a2) {

    // L = light/incoming direction aka wi
    // V = view/outgoing direction aka wo
    // N = surface normal aka (0, 1, 0) bc working in tangent space

    float dotNL = wi.y;
    float dotNV = wo.y;

    float denomA = dotNV * sqrt(a2 + (1.0f - a2) * dotNL * dotNL);
    float denomB = dotNL * sqrt(a2 + (1.0f - a2) * dotNV * dotNV);

    return 2.0f * dotNL * dotNV / (denomA + denomB);
}

__host__ __device__ float clamp(
    float value,
    float min,
    float max
) {
    if (value < min) {
        return min;
    }
    else if (value > max) {
        return max;
    }

    return value;
}

__host__ __device__ void sampleGGXNorm(
    const Material &m,
    glm::vec3 wo, 
    glm::vec3 &wi,
    glm::vec3 &reflectance,
    thrust::default_random_engine &rng) {
    // theta = angle between microfacet normal & actual surface normal
    // lower roughness -> smaller theta, higher roughness -> larger theta
    // phi = direction geometric normal tilts from surface normal 

    // wi = direction of incoming ray in light transport equation 
    // want to choose direction wi
    // wo = outgoing ray direction 
    // wg = geometric normal from the surface

    float a2 = m.roughness * m.roughness;

    thrust::uniform_real_distribution<float> u01(0, 1);
    float e0 = u01(rng);
    float e1 = u01(rng);

    float cosTheta = sqrt((1.0f - e0) / ((a2 - 1.0f) * e0 + 1.0f));
    cosTheta = clamp(cosTheta, 0.0f, 1.0f);

    float theta = acos(cosTheta);
    float phi = TWO_PI * e1;

    // microfacet normal 
    glm::vec3 wm = sphericalToCartesian(theta, phi);

    // calculate wi by reflecting wo about wm
    wi = 2.0f * glm::dot(wo, wm) * wm - wo;

    // want tangent space (normal is (0, 1, 0))
    // wi.y is the dot product w/ normal

    float wiwm = glm::dot(wi, wm);
    if (wi.y > 0.0f && wiwm > 0.0f) {

        // more reflected at grazing angles
        glm::vec3 F = schlickFresnel(m.color, wiwm);
        // less reflecting when there is shadowing/masking 
        float G = smithGGXMaskingShadowing(wi, wo, a2);

        float weight = abs(glm::dot(wo, wm)) / (wo.y * wm.y);

        // takes pdf into account 
        // calculating light comtribution of wi 
        // for diffuse lobe, estimator simplifies to just the albedo itself 
        reflectance = F * G * weight;
    }
    else {
        reflectance = glm::vec3(0.f);
    }
}

// light sampling 

/*__host__ __device__ void uniformSampleOneLight(
    const int numLights,
    const glm::vec3 &ref,
    const Material *materials,
    const Geom* lights,
    thrust::default_random_engine& rng) {

    if (numLights == 0) return;

    // uniformly sample a light in the scene 
    int light_i = min((int)(u01(rng) * numLights), numLights - 1);
    const Geom& light = lights[light_i];

    glm::vec3 wi;
    float pdf;
    // sample point on light and return corresponding wi and pdf
    
    switch (light.type) {
        case LightType::AREA:
            sampleIncomingAreaLight();
    }


}*/

__host__ __device__ void scatterRay(
    PathSegment &pathSegment,
    glm::vec3 intersect,
    glm::vec3 normal,
    const Material &m,
    thrust::default_random_engine &rng)
{
    // TODO: implement this.
    // A basic implementation of pure-diffuse shading will just call the
    // calculateRandomDirectionInHemisphere defined above.

    // have to update the pathSegment
    // ray direction, color contribution, remainingbounces
    thrust::uniform_real_distribution<float> u01(0, 1);

    // pbr metallic workflow model 

    // build frame around normal 
    glm::vec3 up = normal;
    glm::vec3 directionNotNormal;
    if (abs(normal.x) < SQRT_OF_ONE_THIRD)
    {
        directionNotNormal = glm::vec3(1, 0, 0);
    }
    else if (abs(normal.y) < SQRT_OF_ONE_THIRD)
    {
        directionNotNormal = glm::vec3(0, 1, 0);
    }
    else
    {
        directionNotNormal = glm::vec3(0, 0, 1);
    }
    glm::vec3 t = glm::normalize(glm::cross(up, directionNotNormal));
    glm::vec3 b = glm::normalize(glm::cross(up, t));

    // transformation local -> world
    glm::mat3 toWorld(t, up, b);

    glm::vec3 wo = glm::inverse(toWorld) * (-pathSegment.ray.direction);

    float F;
    if (m.metallic > 0.5f) {
        glm::vec3 metallicF = schlickFresnel(m.color, glm::dot(normal, wo));
        F = metallicF.g; // ??
    }
    else {
        // approximate for dielectric (for now?) 
        F = 0.04;
    }

    float p = u01(rng);
    // sample ray 
    glm::vec3 wi; 
    glm::vec3 reflectance;

    if (m.metallic > 0.5f) { // sample specular lobe 
        sampleGGXNorm(m, wo, wi, reflectance, rng);
    }
    else { // sample diffuse lobe 
        wi = calculateRandomDirectionInHemisphere(normal, rng);
        reflectance = m.color;
    }

    pathSegment.ray.origin = intersect + normal * EPSILON;
    pathSegment.ray.direction = toWorld * wi;
    pathSegment.color *= reflectance;
    pathSegment.remainingBounces--;
}

__host__ __device__ void scatterRayFake(
    PathSegment& pathSegment,
    glm::vec3 intersect,
    glm::vec3 normal,
    const Material& m,
    thrust::default_random_engine& rng) {

    glm::vec3 dir = glm::vec3(0.f, 0.f, 0.f);
    if (m.hasReflective == 1.0f) {
        dir = calculateReflectedRayDirection(normal, pathSegment.ray.direction);
    }
    else {
        dir = calculateRandomDirectionInHemisphere(normal, rng);
    }

    pathSegment.ray.origin = intersect + normal * EPSILON;
    pathSegment.ray.direction = dir;

    glm::vec3 color = m.color;
    pathSegment.color *= color;
    pathSegment.remainingBounces--;
}