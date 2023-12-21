//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#ifndef BRDF_HLSL_
#define BRDF_HLSL_

template<typename T> T Square(in T val)
{
    return val * val;
}

float IORToF0(in float eta1, in float eta2)
{
    return Square(eta2 - eta1) / Square(eta2 + eta1);
}

float3 IORToF0(in float3 eta1, in float3 eta2)
{
    return Square(eta2 - eta1) / Square(eta2 + eta1);
}

float IORToF0Air(in float eta)
{
    return IORToF0(1.0f, eta);
}

float F0ToIOR(in float f0)
{
    return (sqrt(f0) + 1) / (1.0001f - sqrt(f0));
}

float3 F0ToIOR(in float3 f0)
{
    return (sqrt(f0) + 1) / (1.0001f - sqrt(f0));
}

//-------------------------------------------------------------------------------------------------
// Calculates the Fresnel factor using Schlick's approximation
//-------------------------------------------------------------------------------------------------
float3 Fresnel(in float3 specularF0, in float cosTheta)
{
    float3 fresnel = specularF0 + (1.0f - specularF0) * pow((1.0f - cosTheta), 5.0f);

    // Fade out spec entirely when lower than 0.1% albedo
    fresnel *= saturate(dot(specularF0, 333.0f));

    return fresnel;
}

//-------------------------------------------------------------------------------------------------
// Calculates the Fresnel factor using Schlick's approximation
//-------------------------------------------------------------------------------------------------
float Fresnel(in float specularF0, in float cosTheta)
{
    float fresnel = specularF0 + (1.0f - specularF0) * pow((1.0f - cosTheta), 5.0f);

    // Fade out spec entirely when lower than 0.1% albedo
    fresnel *= saturate(specularF0 * 1000.0f);

    return fresnel;
}

//-------------------------------------------------------------------------------------------------
// Calculates the Fresnel factor using Schlick's approximation
//-------------------------------------------------------------------------------------------------
float3 Fresnel(in float3 specularF0, in float3 h, in float3 l)
{
    float3 fresnel = specularF0 + (1.0f - specularF0) * pow((1.0f - saturate(dot(l, h))), 5.0f);

    // Fade out spec entirely when lower than 0.1% albedo
    fresnel *= saturate(dot(specularF0, 333.0f));

    return fresnel;
}

//-------------------------------------------------------------------------------------------------
// Calculates the Fresnel factor using Schlick's approximation
//-------------------------------------------------------------------------------------------------
float3 Fresnel(in float3 specularF0, in float3 fresnelAlbedo, in float3 h, in float3 l)
{
    float3 fresnel = specularF0 + (fresnelAlbedo - specularF0) * pow((1.0f - saturate(dot(l, h))), 5.0f);

    // Fade out spec entirely when lower than 0.1% albedo
    fresnel *= saturate(dot(specularF0, 333.0f));

    return fresnel;
}

//-------------------------------------------------------------------------------------------------
// Calculates the refraction vector but ignores total interal refraction and avoids NaNs
//-------------------------------------------------------------------------------------------------
float3 Refract(in float3 incident, float3 normal, float eta)
{
    float nDotI = dot(normal, incident);
    float k = saturate(1.0f - eta * eta * (1.f - nDotI * nDotI));
    return eta * incident - (eta * nDotI + sqrt(k)) * normal;
}

//-------------------------------------------------------------------------------------------------
// Helper for computing the Beckmann geometry term
//-------------------------------------------------------------------------------------------------
float BeckmannG1(float m, float nDotX)
{
    float nDotX2 = nDotX * nDotX;
    float tanTheta = sqrt((1 - nDotX2) / nDotX2);
    float a = 1.0f / (m * tanTheta);
    float a2 = a * a;

    float g = 1.0f;
    if(a < 1.6f)
        g *= (3.535f * a + 2.181f * a2) / (1.0f + 2.276f * a + 2.577f * a2);

    return g;
}

//-------------------------------------------------------------------------------------------------
// Computes the specular term using a Beckmann microfacet distribution, with a matching
// geometry factor and visibility term. Based on "Microfacet Models for Refraction Through
// Rough Surfaces" [Walter 07]. m is roughness, n is the surface normal, h is the half vector,
// l is the direction to the light source, and specAlbedo is the RGB specular albedo
//-------------------------------------------------------------------------------------------------
float BeckmannSpecular(in float m, in float3 n, in float3 h, in float3 v, in float3 l)
{
    float nDotH = max(dot(n, h), 0.0001f);
    float nDotL = saturate(dot(n, l));
    float nDotV = max(dot(n, v), 0.0001f);

    float nDotH2 = nDotH * nDotH;
    float nDotH4 = nDotH2 * nDotH2;
    float m2 = m * m;

    // Calculate the distribution term
    float tanTheta2 = (1 - nDotH2) / nDotH2;
    float expTerm = exp(-tanTheta2 / m2);
    float d = expTerm / (Pi * m2 * nDotH4);

    // Calculate the matching geometric term
    float g1i = BeckmannG1(m, nDotL);
    float g1o = BeckmannG1(m, nDotV);
    float g = g1i * g1o;

    return d * g * (1.0f / (4.0f * nDotL * nDotV));
}


//-------------------------------------------------------------------------------------------------
// Helper for computing the GGX visibility term
//-------------------------------------------------------------------------------------------------
float GGXV1(in float m2, in float nDotX)
{
    return 1.0f / (nDotX + sqrt(m2 + (1 - m2) * nDotX * nDotX));
}

//-------------------------------------------------------------------------------------------------
// Computes the GGX visibility term
//-------------------------------------------------------------------------------------------------
float GGXVisibility(in float m2, in float nDotL, in float nDotV)
{
    return GGXV1(m2, nDotL) * GGXV1(m2, nDotV);
}

float SmithGGXMasking(float3 n, float3 l, float3 v, float a2)
{
    float dotNL = saturate(dot(n, l));
    float dotNV = saturate(dot(n, v));
    float denomC = sqrt(a2 + (1.0f - a2) * dotNV * dotNV) + dotNV;

    return 2.0f * dotNV / denomC;
}

float SmithGGXMaskingShadowing(float3 n, float3 l, float3 v, float a2)
{
    float dotNL = saturate(dot(n, l));
    float dotNV = saturate(dot(n, v));

    float denomA = dotNV * sqrt(a2 + (1.0f - a2) * dotNL * dotNL);
    float denomB = dotNL * sqrt(a2 + (1.0f - a2) * dotNV * dotNV);

    return 2.0f * dotNL * dotNV / (denomA + denomB);
}

//-------------------------------------------------------------------------------------------------
// Computes the specular term using a GGX microfacet distribution, with a matching
// geometry factor and visibility term. Based on "Microfacet Models for Refraction Through
// Rough Surfaces" [Walter 07]. m is roughness, n is the surface normal, h is the half vector,
// l is the direction to the light source, and specAlbedo is the RGB specular albedo
//-------------------------------------------------------------------------------------------------
float GGXSpecular(in float m, in float3 n, in float3 h, in float3 v, in float3 l)
{
    float nDotH = saturate(dot(n, h));
    float nDotL = saturate(dot(n, l));
    float nDotV = saturate(dot(n, v));

    float nDotH2 = nDotH * nDotH;
    float m2 = m * m;

    // Calculate the distribution term
    float x = nDotH * nDotH * (m2 - 1) + 1;
    float d = m2 / (Pi * x * x);

    // Calculate the matching visibility term
    float vis = GGXVisibility(m2, nDotL, nDotV);

    return d * vis;
}

float GGXSpecularAnisotropic(float m, float3 n, float3 h, float3 v, float3 l, float3 tx, float3 ty, float anisotropy)
{
    float nDotH = saturate(dot(n, h));
    float nDotL = saturate(dot(n, l));
    float nDotV = saturate(dot(n, v));
    float nDotH2 = nDotH * nDotH;

    float ax = m;
    float ay = lerp(ax, 1.0f, anisotropy);
    float ax2 = ax * ax;
    float ay2 = ay * ay;

    float xDotH = dot(tx, h);
    float yDotH = dot(ty, h);
    float xDotH2 = xDotH * xDotH;
    float yDotH2 = yDotH * yDotH;

    // Calculate the distribution term
    float denom = (xDotH2 / ax2) + (yDotH2 / ay2) + nDotH2;
    denom *= denom;
    float d = (1.0f / (Pi * ax * ay)) * 1.0f / denom;

    // Calculate the matching visibility term
    float vis = GGXVisibility(m * m, nDotL, nDotV);

    return d * vis;
}

// Distribution term for the velvet BRDF
float VelvetDistribution(in float m, in float nDotH2, in float offset)
{
    m = 0.25f + 0.75f * m;
    float cot2 = nDotH2 / (1.000001f - nDotH2);
    float sin2 = 1.0f - nDotH2;
    float sin4 = sin2 * sin2;
    float amp = 4.0f;
    float m2 = m * m + 0.000001f;
    float cnorm = 1.0f / (Pi * (offset + amp * m2));

    return cnorm * (offset + (amp * exp(-cot2 / (m2 + 0.000001f)) / sin4));
}

// Specular term for the velvet BRDF
float VelvetSpecular(in float m, in float3 n, in float3 h, in float3 v, in float3 l, in float offset)
{
    float nDotH = saturate(dot(n, h));
    float nDotH2 = nDotH * nDotH;
    float nDotV = saturate(dot(n, v));
    float nDotL = saturate(dot(n, l));

    float D = VelvetDistribution(m, nDotH2, offset);
    float G = 1.0f;
    float denom = 1.0f / (4.0f * (nDotL + nDotV - nDotL * nDotV));
    return D * G * denom;
}

//-------------------------------------------------------------------------------------------------
// Returns an adjusted scale factor for environment specular reflections that represents the
// integral of the geometry/visibility + fresnel terms for a GGX BRDF given a particular
// viewing angle and roughness value. The final value is computed using polynomials that were
// fitted to tabulated data generated via monte carlo integration.
//-------------------------------------------------------------------------------------------------
float3 GGXEnvironmentBRDF(in float3 specAlbedo, in float nDotV, in float sqrtRoughness)
{
    const float nDotV2 = nDotV * nDotV;
    const float sqrtRoughness2 = sqrtRoughness * sqrtRoughness;
    const float sqrtRoughness3 = sqrtRoughness2 * sqrtRoughness;

    const float delta = 0.991086418474895f + (0.412367709802119f * sqrtRoughness * nDotV2) -
                        (0.363848256078895f * sqrtRoughness2) -
                        (0.758634385642633f * nDotV * sqrtRoughness2);
    const float bias = saturate((0.0306613448029984f * sqrtRoughness) + 0.0238299731830387f /
                                (0.0272458171384516f + sqrtRoughness3 + nDotV2) -
                                0.0454747751719356f);

    const float scale = saturate(delta - bias);
    return specAlbedo * scale + bias;
}

//-------------------------------------------------------------------------------------------------
// Calculates the lighting result for an analytical light source
//-------------------------------------------------------------------------------------------------
float3 CalcLighting(in float3 normal, in float3 lightDir, in float3 peakIrradiance,
                    in float3 diffuseAlbedo, in float3 specularF0, in float roughness,
                    in float3 positionWS, in float3 cameraPosWS)
{
    float3 lighting = diffuseAlbedo * (1.0f / 3.14159f);

    float3 view = normalize(cameraPosWS - positionWS);
    const float nDotL = saturate(dot(normal, lightDir));
    if(nDotL > 0.0f)
    {
        const float nDotV = saturate(dot(normal, view));
        float3 h = normalize(view + lightDir);

        float3 fresnel = Fresnel(specularF0, h, lightDir);

        float specular = GGXSpecular(roughness, normal, h, view, lightDir);
        lighting += specular * fresnel;
    }

    return lighting * nDotL * peakIrradiance;
}

//-------------------------------------------------------------------------------------------------
// Returns a modifed direction for approximating a sun/directional light as an area light
//-------------------------------------------------------------------------------------------------
float3 DirLightAreaLightApproximation(in float3 lightDirection, in float3 viewDir, in float3 normal,
                                      in float sinAngularRadius, in float cosAngularRadius)
{
    float3 D = lightDirection;
    float3 R = reflect(-viewDir, normal);
    float r = sinAngularRadius;
    float d = cosAngularRadius;
    float3 DDotR = dot(D, R);
    float3 S = R - DDotR * D;
    return select(DDotR < d, normalize(d * D + normalize(S) * r), R);
}

#endif // BRDF_HLSL_