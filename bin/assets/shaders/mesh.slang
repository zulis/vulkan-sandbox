// ---------------------------------------------------------------------------
// mesh.slang — Slang port of mesh.shd for Vulkan
// ---------------------------------------------------------------------------

// --- Vertex input -----------------------------------------------------------
struct VSInput
{
    [[vk::location(0)]]
    float3 position;
    [[vk::location(1)]]
    float3 normal;
    [[vk::location(2)]]
    float2 texCoord;
    [[vk::location(3)]]
    float3 tangent;
    [[vk::location(4)]]
    float3 bitangent;
};

// --- Vertex → Fragment ------------------------------------------------------
struct VSOutput
{
    [[vk::location(0)]]
    float2 texCoord;
    [[vk::location(1)]]
    float3 worldPosition;
    [[vk::location(2)]]
    float3 worldNormal;
    [[vk::location(3)]]
    float3 worldTangent;
    [[vk::location(4)]]
    float3 worldBitangent;
    float4 svPosition : SV_Position;
};

// --- Uniform buffer: transform (set 0, binding 0) ---------------------------
struct TransformData
{
    float4x4 projection;
    float4x4 view;
    float4x4 model;
    float3x3 normalMatrix;
};

[[vk::binding(0, 0)]]
ConstantBuffer<TransformData> gTransform;

// --- Uniform buffer: scene / lights (set 0, binding 1) ----------------------
struct LightInfo
{
    bool enabled;
    int type;
    float3 ambient;
    float3 diffuse;
    float3 specular;
    float3 position;
    float3 direction;
    float3 attenuation;
    float spotCutoff;
    float spotExponent;
    bool usesAttenuation;
    bool usesSpotCone;
};

struct SceneData
{
    float3 viewPos;
    float shininess;
    LightInfo lights[3];
};

[[vk::binding(1, 0)]]
ConstantBuffer<SceneData> gScene;

// --- Textures + sampler (set 1) ---------------------------------------------
[[vk::binding(0, 1)]]
Texture2D tColorMap : register(t0);
[[vk::binding(1, 1)]]
Texture2D tNormalMap : register(t1);
[[vk::binding(2, 1)]]
Texture2D tSpecularMap : register(t2);
[[vk::binding(3, 1)]]
Texture2D tAoMap : register(t3);
[[vk::binding(4, 1)]]
Texture2D tEmissiveMap : register(t4);
[[vk::binding(5, 1)]]
SamplerState sLinear : register(s0);

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static const int LightTypeDirectional = 0;

// ---------------------------------------------------------------------------
// Helper functions (shared between stages)
// ---------------------------------------------------------------------------
float computeAttenuation(LightInfo light, float distanceToLight)
{
    if (!light.usesAttenuation)
        return 1.0;

    float falloff = light.attenuation.x + light.attenuation.y * distanceToLight + light.attenuation.z * distanceToLight * distanceToLight;
    return 1.0 / max(falloff, 0.0001);
}

float computeSpotFactor(LightInfo light, float3 lightDir)
{
    if (!light.usesSpotCone)
        return 1.0;

    float theta = dot(normalize(-lightDir), normalize(light.direction));
    float cutoff = cos(radians(light.spotCutoff));

    if (theta <= cutoff)
        return 0.0;

    if (light.spotExponent <= 0.0)
        return 1.0;

    return pow(theta, light.spotExponent);
}

float3 evaluateLight(
    LightInfo light,
    float3 normal,
    float3 viewDir,
    float3 albedo,
    float3 specularColor,
    float ambientOcclusion,
    float3 worldPosition)
{
    if (!light.enabled)
        return float3(0.0);

    float3 lightDir;
    float distanceToLight = 0.0;

    if (light.type == LightTypeDirectional)
    {
        lightDir = normalize(-light.direction);
    }
    else
    {
        float3 lightVector = light.position - worldPosition;
        distanceToLight = length(lightVector);
        lightDir = distanceToLight > 0.0001
                       ? lightVector / distanceToLight
                       : float3(0.0, 1.0, 0.0);
    }

    float attenuation = computeAttenuation(light, distanceToLight);
    float spotFactor = computeSpotFactor(light, lightDir);
    float diffuseFactor = max(dot(normal, lightDir), 0.0);

    float3 halfwayDir = normalize(lightDir + viewDir);
    float specularFactor = diffuseFactor > 0.0
                               ? pow(max(dot(normal, halfwayDir), 0.0), gScene.shininess)
                               : 0.0;

    float3 ambient = light.ambient * albedo * ambientOcclusion;
    float3 diffuse = light.diffuse * diffuseFactor * albedo;
    float3 specular = light.specular * specularFactor * specularColor;

    return ambient + (diffuse + specular) * attenuation * spotFactor;
}

// ===========================================================================
// Vertex Shader
// ===========================================================================
[shader("vertex")]
VSOutput vertexMain(VSInput input)
{
    TransformData t = gTransform;

    float3 transformedNormal = normalize(mul(t.normalMatrix, input.normal));
    float3 transformedTangent = mul(t.normalMatrix, input.tangent);
    float3 transformedBitangent = mul(t.normalMatrix, input.bitangent);

    if (length(transformedTangent) < 0.0001)
    {
        transformedTangent = abs(transformedNormal.z) < 0.999
                                 ? cross(transformedNormal, float3(0.0, 0.0, 1.0))
                                 : cross(transformedNormal, float3(0.0, 1.0, 0.0));
    }

    transformedTangent = normalize(
        transformedTangent - dot(transformedTangent, transformedNormal) * transformedNormal);

    if (length(transformedBitangent) < 0.0001)
    {
        transformedBitangent = cross(transformedNormal, transformedTangent);
    }

    transformedBitangent = normalize(
        transformedBitangent - dot(transformedBitangent, transformedNormal) * transformedNormal);

    VSOutput output;
    output.texCoord = input.texCoord;
    output.worldNormal = transformedNormal;
    output.worldTangent = transformedTangent;
    output.worldBitangent = transformedBitangent;

    float4 modelPosition = mul(t.model, float4(input.position, 1.0));
    output.worldPosition = modelPosition.xyz;
    output.svPosition = mul(t.projection, mul(t.view, modelPosition));

    return output;
}

// ===========================================================================
// Fragment Shader
// ===========================================================================
float3 sampleNormal(float3 worldTangent, float3 worldBitangent, float3 worldNormal, float2 uv)
{
    float3x3 tbn = float3x3(
        normalize(worldTangent),
        normalize(worldBitangent),
        normalize(worldNormal));

    float3 mappedNormal = tNormalMap.Sample(sLinear, uv).rgb * 2.0 - 1.0;
    return normalize(mul(mappedNormal, tbn));
}

[shader("fragment")]
float4 fragmentMain(VSOutput input)
    : SV_Target
{
    float3 albedo = tColorMap.Sample(sLinear, input.texCoord).rgb;
    float3 specularColor = tSpecularMap.Sample(sLinear, input.texCoord).rgb;
    float ambientOcclusion = tAoMap.Sample(sLinear, input.texCoord).r;
    float3 emissive = tEmissiveMap.Sample(sLinear, input.texCoord).rgb;

    float3 normal = sampleNormal(
        input.worldTangent, input.worldBitangent, input.worldNormal, input.texCoord);
    float3 viewDir = normalize(gScene.viewPos - input.worldPosition);

    float3 lighting = emissive;

    for (int i = 0; i < 3; ++i)
    {
        lighting += evaluateLight(
            gScene.lights[i], normal, viewDir, albedo, specularColor, ambientOcclusion,
            input.worldPosition);
    }

    return float4(lighting, 1.0);
}
