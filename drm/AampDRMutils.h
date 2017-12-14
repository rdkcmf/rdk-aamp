/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
/*

 Data structures to manage DRM sessions

 */

#ifndef AampDRMutils_h
#define AampDRMutils_h
#define MAX_CHALLENGE_LEN 64000

#include <gst/gst.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

class DrmData{

private:
	unsigned char *data;
	int dataLength;
public:

	DrmData(){
		dataLength = 0;
		data = NULL;
	}

	DrmData(unsigned char *data, int dataLength){
		this->data =(unsigned char*) malloc(dataLength + 1);
		this->dataLength = dataLength;
		memcpy(this->data,data,dataLength + 1);
	}
	~DrmData(){
		if(data != NULL)
		{
			free(data);
			data = NULL;
		}
		//printf(" \n\n Releasing data object of length : %d\n\n", dataLength);
	}

	unsigned char * getData(){
		return data;
	}

	int getDataLength(){
		return dataLength;
	}

	void setData(unsigned char * data, int dataLength){
		if(this->data != NULL){
			free(data);
		}
		this->data =  (unsigned char*) malloc(dataLength + 1);
		this->dataLength = dataLength;
		memcpy(this->data,data,dataLength + 1);
	}

	void addData(unsigned char * data, int dataLength){
		if(NULL == this->data){
			this->setData(data,dataLength);
		}
		else{
			this->data = (unsigned char*) realloc(this->data, this->dataLength + dataLength + 1);
			assert(this->data);
			memcpy(&(this->data[this->dataLength]),data,dataLength + 1);
			this->dataLength = this->dataLength + dataLength;
		}
	}

};

#endif
