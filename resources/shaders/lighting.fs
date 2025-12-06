#version 330

// Input vertex attributes (from vertex shader)
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Output fragment color
out vec4 finalColor;

#define     MAX_LIGHTS              4
#define     LIGHT_DIRECTIONAL       0
#define     LIGHT_POINT             1

struct Light {
    int enabled;
    int type;
    vec3 position;
    vec3 target;
    vec4 color;
};

// Input lighting values
uniform Light lights[MAX_LIGHTS];
uniform vec4 ambient;
uniform vec3 viewPos;

void main()
{
    // Texel color fetching from texture sampler
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 lightDot = vec3(0.0);
    vec3 normal = normalize(fragNormal);
    vec3 viewD = normalize(viewPos - fragPosition);
    vec3 specular = vec3(0.0);

    vec4 tint = colDiffuse*fragColor;

    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        if (lights[i].enabled == 1)
        {
            vec3 light = vec3(0.0);
            float attenuation = 1.0;

            if (lights[i].type == LIGHT_DIRECTIONAL)
            {
                light = -normalize(lights[i].target - lights[i].position);
            }

            if (lights[i].type == LIGHT_POINT)
            {
                light = normalize(lights[i].position - fragPosition);
                
                // Calculate attenuation for point light
                float distance = length(lights[i].position - fragPosition);
                attenuation = 1.0 / (1.0 + 0.5 * distance + 0.2 * distance * distance);
            }

            float NdotL = max(dot(normal, light), 0.0);
            lightDot += lights[i].color.rgb * NdotL * attenuation;

            // Enhanced specular for glossy surfaces (like polished mahjong table)
            float specCo = 0.0;
            if (NdotL > 0.0) 
            {
                // Blinn-Phong model for better specular highlights
                vec3 halfDir = normalize(light + viewD);
                specCo = pow(max(0.0, dot(normal, halfDir)), 64.0); // Higher shininess for glossy table
                specular += specCo * lights[i].color.rgb * attenuation * 0.6; // Stronger specular
            }
        }
    }

    // Combine diffuse and specular
    vec3 diffuse = texelColor.rgb * tint.rgb * lightDot;
    vec3 ambientLight = texelColor.rgb * (ambient.rgb / 10.0) * tint.rgb;
    
    finalColor = vec4(diffuse + specular + ambientLight, texelColor.a * tint.a);

    // Gamma correction for more realistic lighting
    finalColor.rgb = pow(finalColor.rgb, vec3(1.0/2.2));
}
