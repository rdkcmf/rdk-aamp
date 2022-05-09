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

/**
 * @file AampCacheHandler.h
 * @brief Cache handler for AAMP
 */
 
#ifndef __AAMP_CACHE_HANDLER_H__
#define __AAMP_CACHE_HANDLER_H__

#include <iostream>
#include <memory>
#include <unordered_map>
#include "priv_aamp.h"

#define PLAYLIST_CACHE_SIZE_UNLIMITED -1

/**
 * @brief PlayListCachedData structure to store playlist data
 */
typedef struct playlistcacheddata{
	std::string mEffectiveUrl;
	GrowableBuffer* mCachedBuffer;
	MediaType mFileType;
	bool mDuplicateEntry;

	playlistcacheddata() : mEffectiveUrl(""), mCachedBuffer(NULL), mFileType(eMEDIATYPE_DEFAULT),mDuplicateEntry(false)
	{
	}

	playlistcacheddata(const playlistcacheddata& p) : mEffectiveUrl(p.mEffectiveUrl), mCachedBuffer(p.mCachedBuffer), mFileType(p.mFileType),mDuplicateEntry(p.mDuplicateEntry)
	{
		mCachedBuffer->ptr = p.mCachedBuffer->ptr;
		mCachedBuffer->len = p.mCachedBuffer->len;
		mCachedBuffer->avail = p.mCachedBuffer->avail;
	}

	playlistcacheddata& operator=(const playlistcacheddata &p)
	{
		mEffectiveUrl = p.mEffectiveUrl;
		mCachedBuffer = p.mCachedBuffer;
		mFileType = p.mFileType;
		mDuplicateEntry = p.mDuplicateEntry;
		return *this;
	}

}PlayListCachedData;

/**
 * @brief InitFragCacheStruct to store Init Fragment data
 * Init fragment cache mechanism
 * All types (VID/AUD/SUB/AUX) of Init Fragment maintained in a single Cache Table,
 * and these fragment's url are stored in a Track Queue corrsponding to file type.
 * This queue will be used to count fragments inserted, to remove entry in FIFO order
 * upon exceeding limit with respect to file type.
 * Eg:
 * TrackQ[VID]={"http://sample_domain/vid_qual1.init"}	   umCacheTable={{"http://sample_domain/vid_qual1.init"}, VidUrlData1}
 *             {"http://sample_domain/vid_qual2.init"}					{{"http://sample_domain/vid_qual1.init_redirect"}, VidUrlData1}
 * 			   {"http://sample_domain/vid_qual3.init"}					{{"http://sample_domain/aud_qual1.init"}, AudUrlData1}
 * TrackQ[AUD]={"http://sample_domain/aud_qual1.init"}					{{"http://sample_domain/vid_qual2.init"}, VidUrlData2}
 *             {"http://sample_domain/aud_qual2.init"}					{{"http://sample_domain/aud_qual1.init_redirect"}, AudUrlData1}
 * 			   {"http://sample_domain/aud_qual3.init"}					{{"http://sample_domain/aud_qual3.init"}, AudUrlData3}
 * 																		{{"http://sample_domain/aud_qual2.init"}, AudUrlData2}
 * 																		{{"http://sample_domain/vid_qual2.init_redirect"}, VidUrlData2}
 * 																		{{"http://sample_domain/vid_qual3.init"}, VidUrlData3}
 * 																		{{"http://sample_domain/aud_qual2.init_redirect"}, AudUrlData2}
 * Track queue will not maintain duplicate entry of cache table, so we can have maximum of different init fragments in cache table.
 * As per above eg, TrackQ[VID] size is 3, but cache table has 5 including effective url entry. If we maintain effective url entry
 * in cache queue, we will have only 3 init fragments in diff quality.
 * If cache table reaches max no of cache per track, we remove both main entry and dup entry if present, in FIFO order.
 * 
 * Fragment cache & track queue will be cleared upon exiting from aamp player or from async clear thread.
 */
typedef struct playlistcacheddata InitFragCacheStruct;

/**
 * @brief initfragtrackstruct to store init fragment url per media track in FIFO Queue.
 */
typedef struct initfragtrackstruct
{
	std::queue<std::string> Trackqueue;

	initfragtrackstruct() : Trackqueue()
	{
	}
}InitFragTrackStruct;

/**
 * @class AampCacheHandler
 * @brief Handles Aamp Cahe operations
 */

class AampCacheHandler
{
private:
	typedef std::unordered_map<std::string, PlayListCachedData *> PlaylistCache ;
	typedef std::unordered_map<std::string, PlayListCachedData *>::iterator PlaylistCacheIter;
	PlaylistCache mPlaylistCache;
	int mCacheStoredSize;
	bool mInitialized;
	bool mCacheActive;
	bool mAsyncCacheCleanUpThread;
	bool mAsyncThreadStartedFlag;
	int mMaxPlaylistCacheSize;
	pthread_mutex_t mMutex;
	pthread_mutex_t mCondVarMutex;
	pthread_cond_t mCondVar ;
	pthread_t mAsyncCleanUpTaskThreadId;
	AampLogManager *mLogObj;

	typedef std::unordered_map <std::string, InitFragCacheStruct*> InitFragCache ;
	typedef std::unordered_map <std::string, InitFragCacheStruct*>::iterator InitFragCacheIter;
	typedef std::unordered_map <MediaType, InitFragTrackStruct*, std::hash<int>> CacheTrackQueue;
	typedef std::unordered_map <MediaType, InitFragTrackStruct*, std::hash<int>>::iterator CacheTrackQueueIter;
	InitFragCache umInitFragCache;
	CacheTrackQueue umCacheTrackQ;
	pthread_mutex_t mInitFragMutex;
	bool bInitFragCache;
	int MaxInitCacheSlot;						/**< Max no of init fragment per track */

private:

	/**
	* @brief Initialization Function
	*/
	void Init();

	/**
	* @brief Clear Cache Handler. Exit clean up thread.
	*/
	void ClearCacheHandler();

	/**
	 *	 @brief Thread function for Async Cache clean
	 *
	 *	 @return void
	 */
	void AsyncCacheCleanUpTask();
	/**
	 *	 @brief Thread entry function
	 *
	 *	 @return void
	 */
	static void * AampCacheThreadFunction(void * This) {((AampCacheHandler *)This)->AsyncCacheCleanUpTask(); return NULL;}
	/**
	 *	 @brief Clear playlist cache
	 *
	 *	 @return void
	 */
	void ClearPlaylistCache();
	/**
	 *   @brief AllocatePlaylistCacheSlot Allocate Slot for adding new playlist
	 *   @param[in] fileType - Indicate the type of playlist to store/remove
	 *   @param[in] newLen  - Size required to store new playlist
	 *
	 *   @return bool Success or Failure
	 */
	bool AllocatePlaylistCacheSlot(MediaType fileType,size_t newLen);

	/**
	 *	 @brief Clear init fragment cache & track queue table
	 *
	 *	 @return void
	 */
	void ClearInitFragCache();

	/**
	 *   @brief Removes very first inserted entry ( and duplicate entry, if present)  of given filetype
	 *   from fragment cache table in FIFO order, also removes the corresponding url from track queue.
	 * 
	 *   @param fileType type of file format to be removed from cache table
	 * 
	 *   @return void
	 */
	void RemoveInitFragCacheEntry ( MediaType fileType );

public:

	/**
	 *	 @brief Default Constructor
	 *
	 *	 @return void
	 */
	AampCacheHandler(AampLogManager *logObj);

	/**
	* @brief Destructor Function
	*/
	~AampCacheHandler();

	/**
	 *	 @brief Start playlist caching
	 *
	 *	 @return void
	 */
	void StartPlaylistCache();
	/**
	 *	 @brief Stop playlist caching
	 *
	 *	 @return void
	 */
	void StopPlaylistCache();

	/**
	 *   @brief Insert playlist into cache
	 *
	 *   @param[in] url - URL
	 *   @param[in] buffer - Pointer to growable buffer
	 *   @param[in] effectiveUrl - Final URL
	 *   @param[in] trackLiveStatus - Live Status of the track inserted
	 *   @param[in] fileType - Type of the file inserted
     	 *
	 *   @return void
	 */
	void InsertToPlaylistCache(const std::string url, const GrowableBuffer* buffer, std::string effectiveUrl,bool trackLiveStatus,MediaType fileType=eMEDIATYPE_DEFAULT);

	/**
	 *   @brief Retrieve playlist from cache
	 *
	 *   @param[in] url - URL
	 *   @param[out] buffer - Pointer to growable buffer
	 *   @param[out] effectiveUrl - Final URL
	 *   @return true: found, false: not found
	 */
	bool RetrieveFromPlaylistCache(const std::string url, GrowableBuffer* buffer, std::string& effectiveUrl);

	/**
	*   @brief SetMaxPlaylistCacheSize - Set Max Cache Size
	*
	*   @param[in] maxPlaylistCacheSz - CacheSize
	*   @return None
	*/
	void SetMaxPlaylistCacheSize(int maxPlaylistCacheSz);
	/**
	*   @brief GetMaxPlaylistCacheSize - Get present CacheSize
	*
	*   @return int - maxCacheSize
	*/
	int  GetMaxPlaylistCacheSize() { return mMaxPlaylistCacheSize; }
	/**
	*   @brief IsUrlCached - Check if URL is already cached
	*
	*   @return bool - true if file found, else false
	*/
	bool IsUrlCached(std::string);

	/**
	 *   @brief Insert init fragment into cache table
	 *
	 *   @param[in] url - URL
	 *   @param[in] buffer - Pointer to growable buffer
	 *   @param[in] effectiveUrl - Final URL
	 *   @param[in] fileType - Type of the file inserted
     	 *
	 *   @return void
	 */
	void InsertToInitFragCache(const std::string url, const GrowableBuffer* buffer, std::string effectiveUrl,MediaType fileType);

	/**
	 *   @brief Retrieve init fragment from cache
	 *
	 *   @param[in] url - URL
	 *   @param[out] buffer - Pointer to growable buffer
	 *   @param[out] effectiveUrl - Final URL
	 * 
	 *   @return true: found, false: not found
	 */
	bool RetrieveFromInitFragCache(const std::string url, GrowableBuffer* buffer, std::string& effectiveUrl);

	/**
	*   @brief SetMaxInitFragCacheSize - Set Max Cache Size
	*
	*   @param[in] maxInitFragCacheSz - CacheSize
	*
	*   @return None
	*/
	void SetMaxInitFragCacheSize( int maxInitFragCacheSz);

	/**
	*   @brief GetMaxPlaylistCacheSize - Get present CacheSize
	*
	*   @return int - maxCacheSize
	*/
	int  GetMaxInitFragCacheSize() { return MaxInitCacheSlot; }

        /**
         * @brief Copy constructor disabled
         *
         */
	AampCacheHandler(const AampCacheHandler&) = delete;
	/**
         * @brief assignment operator disabled
         *
         */
	AampCacheHandler& operator=(const AampCacheHandler&) = delete;
};


#endif
