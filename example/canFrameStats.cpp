/*
 Author: Matt Bunting
 Copyright (c) 2020 Arizona Board of Regents
 All rights reserved.

 Permission is hereby granted, without written agreement and without
 license or royalty fees, to use, copy, modify, and distribute this
 software and its documentation for any purpose, provided that the
 above copyright notice and the following two paragraphs appear in
 all copies of this software.

 IN NO EVENT SHALL THE ARIZONA BOARD OF REGENTS BE LIABLE TO ANY PARTY
 FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 IF THE UNIVERSITY OF CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF
 SUCH DAMAGE.

 THE ARIZONA BOARD OF REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE PROVIDED HEREUNDER
 IS ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION
 TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

 */

#include "canFrameStats.h"

#include <cstring>
#include <algorithm>
#include <unistd.h>

CanFrameStats::CanFrameStats() {
}
CanFrameStats::~CanFrameStats() {
}

void CanFrameStats::newDataNotification(Panda::CanFrame* canFrame) {

	lock();
	Panda::CanFrame* newFrame = new Panda::CanFrame;
	canFrameFifo.push_back(newFrame);
	*newFrame = *canFrame;
	unlock();

}

bool sortId(const IdInfo *i, const IdInfo *j) {
	return i->ID < j->ID;
}
bool sortIdCount(const IdInfo *i, const IdInfo *j) {
	return i->count < j->count || ((i->ID == j->ID) && sortId(i, j));
}
bool sortIdRate(const IdInfo *i, const IdInfo *j) {
	return i->currentRate < j->currentRate || ((i->ID == j->ID) && sortId(i, j));
}
bool sortUniqueMessageCount(const IdInfo *i, const IdInfo *j) {
	return i->data.size() < j->data.size() || ((i->ID == j->ID) && sortId(i, j));
}

void CanFrameStats::sortById() {
	lock();
	std::sort(canStatsSorted.begin(), canStatsSorted.end(), sortId);
	unlock();
}
void CanFrameStats::sortByIdCount() {
	lock();
	std::sort(canStatsSorted.begin(), canStatsSorted.end(), sortIdCount);
	unlock();
}
void CanFrameStats::sortByIdRate() {
	lock();
	std::sort(canStatsSorted.begin(), canStatsSorted.end(), sortIdRate);
	unlock();
}
void CanFrameStats::sortByUniqueMessageCount() {
	lock();
	std::sort(canStatsSorted.begin(), canStatsSorted.end(), sortUniqueMessageCount);
	unlock();

}

void CanFrameStats::doAction() {
	if (canFrameFifo.size() > 0) {


		unsigned long long int data = 0;
		// For packet rate:
		struct timeval sysTime;
		gettimeofday(&sysTime, NULL);
		static struct timeval t0 = sysTime;	// store the initial time
		double currentTime = (double)(sysTime.tv_usec)/1000000.0 + (double)(sysTime.tv_sec - t0.tv_sec);

		lock();
		Panda::CanFrame* canFrame = *canFrameFifo.begin();
		canFrameFifo.pop_front();
		memcpy(&data, canFrame->data, canFrame->dataLength);
		IdInfo* messageStats = &canStats[canFrame->messageID];
		DataInfo* dataStats = &messageStats->data[data];

		messageStats->ID = canFrame->messageID;
		messageStats->count++;

		dataStats->count++;
		dataStats->length = canFrame->dataLength;
		dataStats->bus = canFrame->bus;

		if (canStats.size() > canStatsSorted.size()) { // should work in all cases?
			canStatsSorted.push_back(messageStats);
		}

		if(rateRefreshSecondTracker < sysTime.tv_sec) {	// refresh rates once a second
			for (std::map<unsigned int, IdInfo>::iterator it = canStats.begin(); it != canStats.end(); it++) {
				IdInfo* idInfo = &it->second;
				if (idInfo->priorTime != 0) {
					// simple alpha-LPF-filtered rate, using harmonic mean method
					idInfo->currentRate = 1.0/(( 1.0/idInfo->currentRate ) * 0.90 + 0.10 * (currentTime - idInfo->priorTime) );
				}
			}
			rateRefreshSecondTracker = sysTime.tv_sec;
		} else {
			if (messageStats->priorTime != 0) {
				// simple alpha-LPF-filtered rate, using harmonic mean method
				messageStats->currentRate = 1.0/(( 1.0/messageStats->currentRate ) * 0.90 + 0.10 * (currentTime - messageStats->priorTime) );
			}
		}
		messageStats->priorTime = currentTime;

		delete canFrame;
		unlock();
		return;
	}
	usleep(10);
}

