/* This code has been published on learnopengl.com by JOEY DE VRIES and is available under
* the following license: https://creativecommons.org/licenses/by-nc/4.0/legalcode
*/

#version 330 core
out vec4 FragColor;

in vec3 ourColor;
in vec2 TexCoord;

// texture sampler
uniform sampler2D texture1;

void main()
{
	FragColor = texture(texture1, TexCoord);
}
