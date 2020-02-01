#pragma once

#include <memory>


// This method only works in debug without optimisations
//#define SAIL_COMPONENT static int getStaticID() { \
//	return reinterpret_cast<int>(&getStaticID); \
//}

// Creates a unique id for each class which derives from component
// This method uses the gcc defined __COUNTER__ macro that increments with every use
#define SAIL_COMPONENT static int getStaticID() { \
	return __COUNTER__; \
}

class SailGuiWindow;

class Component {
public:
	typedef std::unique_ptr<Component> Ptr;
public:
	Component() {}
	virtual ~Component() {}
	virtual void renderEditorGui(SailGuiWindow* window) {};

private:
};

