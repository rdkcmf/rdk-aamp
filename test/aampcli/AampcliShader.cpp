/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file AampcliShader.cpp
 * @brief Aampcli shader file.
 */

#include "AampcliShader.h"

#ifdef RENDER_FRAMES_IN_APP_CONTEXT

static const char *VSHADER =
"attribute vec2 vertexIn;"
"attribute vec2 textureIn;"
"varying vec2 textureOut;"
"uniform mat4 trans;"
"void main() {"
"gl_Position = trans * vec4(vertexIn,0, 1);"
"textureOut = textureIn;"
"}";

static const char *FSHADER =
"#ifdef GL_ES \n"
"  precision mediump float; \n"
"#endif \n"
"varying vec2 textureOut;"
"uniform sampler2D tex_y;"
"uniform sampler2D tex_u;"
"uniform sampler2D tex_v;"
"void main() {"
"vec3 yuv;"
"vec3 rgb;"
"yuv.x = texture2D(tex_y, textureOut).r;"
"yuv.y = texture2D(tex_u, textureOut).r - 0.5;"
"yuv.z = texture2D(tex_v, textureOut).r - 0.5;"
"rgb = mat3( 1, 1, 1, 0, -0.39465, 2.03211, 1.13983, -0.58060,  0) * yuv;"
"gl_FragColor = vec4(rgb, 1);"
"}";

GLuint Shader::LoadShader( GLenum type )
{
	GLuint shaderHandle = 0;
	const char *sources[1];

	if(GL_VERTEX_SHADER == type)
	{
		sources[0] = VSHADER;
	}
	else
	{
		sources[0] = FSHADER;
	}

	if( sources[0] )
	{
		shaderHandle = glCreateShader(type);
		glShaderSource(shaderHandle, 1, sources, 0);
		glCompileShader(shaderHandle);
		GLint compileSuccess;
		glGetShaderiv(shaderHandle, GL_COMPILE_STATUS, &compileSuccess);
		if (compileSuccess == GL_FALSE)
		{
			GLchar msg[1024];
			glGetShaderInfoLog(shaderHandle, sizeof(msg), 0, &msg[0]);
			printf("[AAMPCLI] %s\n", msg );
		}
	}

	return shaderHandle;
}

void Shader::InitShaders()
{
	GLint linked;

	GLint vShader = LoadShader(GL_VERTEX_SHADER);
	GLint fShader = LoadShader(GL_FRAGMENT_SHADER);
	mProgramID = glCreateProgram();
	glAttachShader(mProgramID,vShader);
	glAttachShader(mProgramID,fShader);

	glBindAttribLocation(mProgramID, ATTRIB_VERTEX, "vertexIn");
	glBindAttribLocation(mProgramID, ATTRIB_TEXTURE, "textureIn");
	glLinkProgram(mProgramID);
	glValidateProgram(mProgramID);

	glGetProgramiv(mProgramID, GL_LINK_STATUS, &linked);
	if( linked == GL_FALSE )
	{
		GLint logLen;
		glGetProgramiv(mProgramID, GL_INFO_LOG_LENGTH, &logLen);
		GLchar *msg = (GLchar *)malloc(sizeof(GLchar)*logLen);
		glGetProgramInfoLog(mProgramID, logLen, &logLen, msg );
		printf( "%s\n", msg );
		free( msg );
	}
	glUseProgram(mProgramID);
	glDeleteShader(vShader);
	glDeleteShader(fShader);

	//Get Uniform Variables Location
	textureUniformY = glGetUniformLocation(mProgramID, "tex_y");
	textureUniformU = glGetUniformLocation(mProgramID, "tex_u");
	textureUniformV = glGetUniformLocation(mProgramID, "tex_v");

	typedef struct _vertex
	{
		float p[2];
		float uv[2];
	} Vertex;

	static const Vertex vertexPtr[4] =
	{
		{{-1,-1}, {0.0,1 } },
		{{ 1,-1}, {1,1 } },
		{{ 1, 1}, {1,0.0 } },
		{{-1, 1}, {0.0,0.0} }
	};
	static const unsigned short index[6] =
	{
		0,1,2, 2,3,0
	};

	glGenVertexArrays(1, &_vertexArray);
	glBindVertexArray(_vertexArray);
	glGenBuffers(2, _vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertexPtr), vertexPtr, GL_STATIC_DRAW );
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _vertexBuffer[1]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index), index, GL_STATIC_DRAW );
	glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, GL_FALSE,
			sizeof(Vertex), (const GLvoid *)offsetof(Vertex,p) );
	glEnableVertexAttribArray(ATTRIB_VERTEX);

	glVertexAttribPointer(ATTRIB_TEXTURE, 2, GL_FLOAT, GL_FALSE,
			sizeof(Vertex), (const GLvoid *)offsetof(Vertex, uv ) );
	glEnableVertexAttribArray(ATTRIB_TEXTURE);
	glBindVertexArray(0);

	glGenTextures(1, &id_y);
	glBindTexture(GL_TEXTURE_2D, id_y);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenTextures(1, &id_u);
	glBindTexture(GL_TEXTURE_2D, id_u);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenTextures(1, &id_v);
	glBindTexture(GL_TEXTURE_2D, id_v);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void Shader::glRender(void){
	/** Input in I420 (YUV420) format.
	 * Buffer structure:
	 * ----------
	 * |        |
	 * |   Y    | size = w*h
	 * |        |
	 * |________|
	 * |   U    |size = w*h/4
	 * |--------|
	 * |   V    |size = w*h/4
	 * ----------*
	 */
	int pixel_w = 0;
	int pixel_h = 0;
	uint8_t *yuvBuffer = NULL;
	unsigned char *yPlane, *uPlane, *vPlane;

	{
		std::lock_guard<std::mutex> lock(appsinkData_mutex);
		yuvBuffer = appsinkData.yuvBuffer;
		appsinkData.yuvBuffer = NULL;
		pixel_w = appsinkData.width;
		pixel_h = appsinkData.height;
	}
	if(yuvBuffer)
	{
		yPlane = yuvBuffer;
		uPlane = yPlane + (pixel_w*pixel_h);
		vPlane = uPlane + (pixel_w*pixel_h)/4;

		glClearColor(0.0,0.0,0.0,0.0);
		glClear(GL_COLOR_BUFFER_BIT);

		//Y
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, id_y);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, pixel_w, pixel_h, 0, GL_RED, GL_UNSIGNED_BYTE, yPlane);
		glUniform1i(textureUniformY, 0);

		//U
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, id_u);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, pixel_w/2, pixel_h/2, 0, GL_RED, GL_UNSIGNED_BYTE, uPlane);
		glUniform1i(textureUniformU, 1);

		//V
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, id_v);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, pixel_w/2, pixel_h/2, 0, GL_RED, GL_UNSIGNED_BYTE, vPlane);
		glUniform1i(textureUniformV, 2);

		//Rotate
		glm::mat4 trans = glm::rotate(
				glm::mat4(1.0f),
				currentAngleOfRotation * 360,
				glm::vec3(1.0f, 1.0f, 1.0f)
				);
		GLint uniTrans = glGetUniformLocation(mProgramID, "trans");
		glUniformMatrix4fv(uniTrans, 1, GL_FALSE, glm::value_ptr(trans));

		glBindVertexArray(_vertexArray);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0 );
		glBindVertexArray(0);

		glutSwapBuffers();
		SAFE_DELETE(yuvBuffer);
	}
}

void Shader::updateYUVFrame(uint8_t *buffer, int size, int width, int height)
{
	uint8_t* frameBuf = new uint8_t[size];
	memcpy(frameBuf, buffer, size);

	{
		std::lock_guard<std::mutex> lock(appsinkData_mutex);
		if(appsinkData.yuvBuffer)
		{
			printf("[AAMPCLI] Drops frame.\n");
			SAFE_DELETE(appsinkData.yuvBuffer);
		}
		appsinkData.yuvBuffer = frameBuf;
		appsinkData.width = width;
		appsinkData.height = height;
	}
}

void Shader::timer(int v) 
{
	currentAngleOfRotation += 0.0001;
	if (currentAngleOfRotation >= 1.0)
	{
		currentAngleOfRotation = 0.0;
	}
	glutPostRedisplay();

	glutTimerFunc(1000/FPS, timer, v);
}
#endif
