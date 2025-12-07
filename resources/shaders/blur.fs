#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform float blurStrength;
uniform vec2 resolution;
uniform vec2 blurCenter;

out vec4 finalColor;

void main()
{
    float strength = max(blurStrength, 0.0);
    if (strength <= 0.0001)
    {
        finalColor = texture(texture0, fragTexCoord) * fragColor;
        return;
    }

    vec2 center = blurCenter;
    vec2 dir = center - fragTexCoord;
    float dist = length(dir);
    dir = normalize(dir + vec2(1e-5));

    int samples = 8;
    float stepSize = 0.015 * strength;

    vec4 accum = texture(texture0, fragTexCoord);
    for (int i = 1; i <= samples; i++)
    {
        float t = float(i) / float(samples);
        vec2 offset = dir * stepSize * t * (1.0 + dist * 1.2);
        accum += texture(texture0, fragTexCoord + offset);
    }

    accum /= float(samples + 1);
    finalColor = accum * fragColor;
}
