/*
 * Trajectory.cpp
 *
 *  Created on: 13.08.2016
 *      Author: JochenAlt
 */

#include "Trajectory.h"
#include "Kinematics.h"

const int TrajectorySampleTime_ms = 100;

Trajectory::Trajectory(const Trajectory& t) {
	trajectory = t.trajectory;
	interpolation = t.interpolation;
	currentTrajectoryNode = t.currentTrajectoryNode;
}
void Trajectory::operator=(const Trajectory& t) {
	trajectory = t.trajectory;
	interpolation = t.interpolation;
	currentTrajectoryNode = t.currentTrajectoryNode;
}

Trajectory::Trajectory() {
	currentTrajectoryNode = -1;// no currently selected node
}

void Trajectory::compile() {
	// update starting times per node
	int currTime_ms = 0;
	interpolation.clear();

	if (trajectory.size() > 1) {
        // clear cache of trajectory nodes with kinematics
		clearCurve();

		// set interpolation
		interpolation.resize(trajectory.size()-1);

		// compute timing first, since this is required for interpolation
		for (unsigned int i = 0;i<trajectory.size();i++) {
			trajectory[i].time_ms = currTime_ms;
			currTime_ms += trajectory[i].duration_ms;
		}

		// compute and save beziercurve between support points
		for (unsigned int i = 0;i<trajectory.size();i++) {
			TrajectoryNode& curr = trajectory[i];
			if (i+1 < trajectory.size()) {
				TrajectoryNode next = trajectory[i+1];
				TrajectoryNode prev;
				TrajectoryNode nextnext;
				if (i>0)
					prev = trajectory[i-1];
				if (i+2 < trajectory.size())
					nextnext = trajectory[i+2];
				interpolation[i].set(prev, curr,next, nextnext); // this computes the bezier curve
			}
		}

		// check for configuration changes
		uint32_t startTime = trajectory[0].time_ms;
		uint32_t endTime = getDurationMS();
		uint32_t time = startTime;
		JointAngles currAngles = trajectory[0].angles;
		PoseConfigurationType currConfiguration;
		Kinematics::computeConfiguration(currAngles, currConfiguration);

		while (time <= endTime) {
			TrajectoryNode node = getSupportXNodeByTime(time, false);
			PoseConfigurationType configuration;
			TrajectoryNode IKNode;
			node.angles = currAngles;
			Kinematics::getInstance().computeInverseKinematics(node.pose, IKNode);
			Kinematics::computeConfiguration(IKNode.angles, configuration);

            // store kinematics in trajectory
            setCurvePoint(time, IKNode);

            currAngles = IKNode.angles;
			currConfiguration = configuration;
			time += TrajectoryPlayerSampleRate_ms;
		}

	}
	if (currentTrajectoryNode >= (int)trajectory.size())
		currentTrajectoryNode = (int)trajectory.size() -1;
}

TrajectoryNode& Trajectory::get(int idx) {
	return trajectory[idx];
};

TrajectoryNode& Trajectory::select(int idx) {
	currentTrajectoryNode = idx;
	return get(idx);
};

int  Trajectory::selected() {
	return currentTrajectoryNode;
}

TrajectoryNode Trajectory::getCurveNodeByTime(int time_ms, bool select) {
	TrajectoryNode result;
	if (isCurveAvailable(time_ms)) {
		result = getCurvePoint(time_ms);
		return result;
	}
	else
		LOG(ERROR) << "curve in trajectory not pre-computed";
	return getSupportXNodeByTime(time_ms, select);
}

TrajectoryNode Trajectory::getSupportXNodeByTime(int time_ms, bool select) {
	// find node that starts right before time_ms
	unsigned int idx = 0;
	while ((idx < trajectory.size()) && (trajectory[idx].time_ms + trajectory[idx].duration_ms< time_ms)) {
		idx++;
	}
	if ((trajectory.size() > 0) && (trajectory[idx].time_ms <= time_ms)) {
		TrajectoryNode result;
		TrajectoryNode startNode= trajectory[idx];
		if (select)
			currentTrajectoryNode = idx;
		if (idx < trajectory.size()-1) {
			BezierCurve bezier = interpolation[idx];
			float t = ((float)time_ms-startNode.time_ms) / ((float)startNode.duration_ms);
			TrajectoryNode node = bezier.getCurrent(t);
			result = node;
		} else {
			result = trajectory[trajectory.size()-1];
		}
		return result;
	}
	return TrajectoryNode();
}

TrajectoryNode Trajectory::getCurvePoint(int time) {
    int idx = time / TrajectoryPlayerSampleRate_ms;
    if (idx < (int)curve.size())
        return curve[idx];
    return TrajectoryNode();
}

bool  Trajectory::isCurveAvailable(int time) {
    int idx = time / TrajectoryPlayerSampleRate_ms;
    if (idx < (int)curve.size()) {
        return (!curve[idx].isNull());
    }
    return false;
}

void Trajectory::setCurvePoint(int time, const TrajectoryNode& node) {
    int idx = time / TrajectoryPlayerSampleRate_ms;
    curve.resize(idx+1);
    curve.at(idx) = node;
}

void Trajectory::clearCurve() {
    curve.clear();
}

unsigned int Trajectory::getDurationMS() {
	int sum_ms = 0;
	for (unsigned i = 0;i<trajectory.size()-1;i++) {
		sum_ms += trajectory[i].duration_ms;
	}
	return sum_ms;
}

void Trajectory::save(string filename) {

	ofstream f(filename);
	string str = marshal(*this);
	f << str;
	f.close();
}

string Trajectory::marshal(const Trajectory& t) {
	stringstream str;
	str.precision(4);

	for (unsigned i = 0;i<t.trajectory.size();i++) {
		TrajectoryNode node = t.trajectory[i];
		str << fixed << "id=" << i << endl;
		str << "name=" << node.name << endl;
		str << "duration=" << node.duration_ms << endl;
		str << "position.x=" << node.pose.position.x << endl;
		str << "position.y=" << node.pose.position.y << endl;
		str << "position.z=" << node.pose.position.z << endl;
		str << "orientation.x=" << node.pose.orientation.x << endl;
		str << "orientation.y=" << node.pose.orientation.y << endl;
		str << "orientation.z=" << node.pose.orientation.z << endl;
		str << "gripper=" << node.pose.gripperAngle << endl;
		str << "interpolation=" << (int)node.interpolationType<< endl;
		str << "angles.0=" << node.angles[0] << endl;
		str << "angles.1=" << node.angles[1] << endl;
		str << "angles.2=" << node.angles[2] << endl;
		str << "angles.3=" << node.angles[3] << endl;
		str << "angles.4=" << node.angles[4] << endl;
		str << "angles.5=" << node.angles[5] << endl;
		str << "angles.6=" << node.angles[6] << endl;

	}

	return str.str();
}

Trajectory  Trajectory::unmarshal(string str) {
	stringstream f(str);
	TrajectoryNode node;
	Trajectory result;
	bool nodePending = false;
	string line;
	while (getline(f, line)) {
		if (string_starts_with(line, "id=")) {
			if (nodePending) {
				result.trajectory.insert(result.trajectory.end(), node);
				nodePending = false;
			}
		} else {
			nodePending = true;
		}
		if (string_starts_with(line, "name=")) {
			char buffer[256];
			buffer[0] = 0;
            sscanf(line.c_str(),"name=%s", &buffer[0]);
            node.name = buffer;
		}
        sscanf(line.c_str(),"duration=%i", &node.duration_ms);
        sscanf(line.c_str(),"position.x=%lf", &node.pose.position.x);
        sscanf(line.c_str(),"position.y=%lf", &node.pose.position.y);
        sscanf(line.c_str(),"position.z=%lf", &node.pose.position.z);
        sscanf(line.c_str(),"orientation.x=%lf", &node.pose.orientation.x);
        sscanf(line.c_str(),"orientation.y=%lf", &node.pose.orientation.y);
        sscanf(line.c_str(),"orientation.z=%lf", &node.pose.orientation.z);
        sscanf(line.c_str(),"gripper=%lf", &node.pose.gripperAngle);
        sscanf(line.c_str(),"interpolation=%i", (int*)&node.interpolationType);
        sscanf(line.c_str(),"angles.0=%lf", &node.angles[0]);
        sscanf(line.c_str(),"angles.1=%lf", &node.angles[1]);
        sscanf(line.c_str(),"angles.2=%lf", &node.angles[2]);
        sscanf(line.c_str(),"angles.3=%lf", &node.angles[3]);
        sscanf(line.c_str(),"angles.4=%lf", &node.angles[4]);
        sscanf(line.c_str(),"angles.5=%lf", &node.angles[5]);
        sscanf(line.c_str(),"angles.6=%lf", &node.angles[6]);
	}
	if (nodePending) {
		result.trajectory.insert(result.trajectory.end(), node);
		nodePending = false;
	}
	return result;
}

void Trajectory::load(string filename) {
	trajectory.clear();
	interpolation.clear();
	currentTrajectoryNode= -1;
	merge(filename);
}

void Trajectory::merge(string filename) {
	ifstream f(filename);
	TrajectoryNode node;
	string str;
	while(!f.eof())
	{
		string line;
		getline(f, line);
		str += line;
		str += LFCR;
	}
	f.close();
	Trajectory toBeMerged = unmarshal(str);
	for (unsigned i = 0;i<toBeMerged.trajectory.size();i++) {
		trajectory.insert(trajectory.end(), toBeMerged.trajectory[i]);
	}

	compile();
}


string Trajectory::toString() const {

	stringstream str;
	str.precision(3);
	str << listStartToString("trajectory", trajectory.size());
	for (unsigned i = 0;i< trajectory.size();i++) {
		str << trajectory[i].toString();
	}
	str << listEndToString();

	return str.str();
}

bool Trajectory::fromString(const string& str, int &idx) {
	int card = 0;
	bool ok = listStartFromString("trajectory", str, card, idx);

	trajectory.clear();
    for (int i = 0;i<card;i++) {
    	TrajectoryNode node;
    	string s = str.substr(idx);
        ok = ok && node.fromString(str,idx);
    	trajectory.insert(trajectory.end(),node);
    }
	ok = ok && listEndFromString(str, idx);

    return ok;
}


