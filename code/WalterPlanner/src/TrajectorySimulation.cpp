/*
 * TrajectorySimulation.cpp
 *
 * Author: JochenAlt
 */

#include "setup.h"
#include "spatial.h"
#include "TrajectorySimulation.h"
#include "Util.h"
#include "WindowController.h"
#include "Kinematics.h"
#include "ExecutionInvoker.h"
#include "logger.h"

// is called by TrajectoryPlayer when a new pose is computed. We inform the UI of the change
void TrajectorySimulation::notifyNewPose(const Pose& pPose) {
	if (WindowController::getInstance().isReady()) {
		WindowController::getInstance().notifyNewBotData();
	}
}

bool poseInputCallback(const Pose& pose) {
	return TrajectorySimulation::getInstance().setPose(pose);
}

// called when angles have been changed in ui and kinematics need to be recomputed
void anglesInputCallback(const JointAngles& pAngles) {
	TrajectorySimulation::getInstance().setAngles(pAngles);
}

TrajectorySimulation::TrajectorySimulation() {
	resetTrajectory();
}

void TrajectorySimulation::setup(int pSampleRate) {
	retrieveFromRealBotFlag = false;
	TrajectoryPlayer::setup(pSampleRate);

	// callbacks from UI: inform me when any data  has changed
	WindowController::getInstance().setTcpInputCallback(poseInputCallback);
	WindowController::getInstance().setAnglesCallback(anglesInputCallback);
	Pose pose;
	pose.angles = getCurrentAngles();
	Kinematics::getInstance().computeForwardKinematics( pose);

	// carry out inverse kinematics to get alternative solutions
	setPose(pose);
}

bool TrajectorySimulation::heartBeatSendOp() {
	if (sendOp) {
		sendOp = false;
		return true;
	}
	return false;
}

bool TrajectorySimulation::heartBeatReceiveOp() {
	if (receiveOp) {
		receiveOp = false;
		return true;
	}
	return false;
}


void TrajectorySimulation::loop() {
	TrajectoryPlayer::loop();

	uint32_t now = millis();
	if ((now >= (uint32_t)lastLoopTime + getSampleRate())) {
		if ((uint32_t)lastLoopTime < now - getSampleRate())
			lastLoopTime = now; // in case timing got screwed up, set last time to now
		else
			lastLoopTime += getSampleRate(); // dont take now, but add diff in order to keep same frequency

		// if the bot is moving, fetch its current position and send it to the UI via Trajectory Simulation
		if (retrieveFromRealBotFlag) {
			TrajectoryNode currentNode = ExecutionInvoker::getInstance().getAngles();
			if (currentNode.isNull())
				LOG(ERROR) << "parse error node";
			else {
				// set pose of bot to current node and send to UI
				TrajectorySimulation::getInstance().setAngles(currentNode.pose.angles);
				receiveOp = true;
			}
		}

		// we need to send the simulation pose to the bot
		if (sendToRealBotFlag) {
			JointAngles currentAngles = TrajectorySimulation::getInstance().getCurrentAngles();
			ExecutionInvoker::getInstance().setAngles(currentAngles);
		}
	}
}

void TrajectorySimulation::receiveFromRealBot(bool yesOrNo) {
	retrieveFromRealBotFlag = yesOrNo;
}

void TrajectorySimulation::sendToRealBot(bool yesOrNo) {
	sendToRealBotFlag = yesOrNo;
}

bool TrajectorySimulation::botIsUpAndRunning() {
	bool isUp = ExecutionInvoker::getInstance().isBotUpAndRunning();
	return isUp;
}

void TrajectorySimulation::setupBot() {
	ExecutionInvoker::getInstance().startupBot();
}

void TrajectorySimulation::teardownBot() {
	ExecutionInvoker::getInstance().teardownBot();
}

