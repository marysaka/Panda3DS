#include "kernel.hpp"
#include <cstring>

Handle Kernel::makePort(const char* name) {
	Handle ret = makeObject(KernelObjectType::Port);
	portHandles.push_back(ret); // Push the port handle to our cache of port handles
	objects[ret].data = new Port(name);

	return ret;
}

Handle Kernel::makeSession(Handle portHandle) {
	const auto port = getObject(portHandle, KernelObjectType::Port);
	if (port == nullptr) [[unlikely]] {
		Helpers::panic("Trying to make session for non-existent port");
	}

	// Allocate data for session
	const Handle ret = makeObject(KernelObjectType::Session);
	objects[ret].data = new Session(portHandle);
	return ret;
}

// Get the handle of a port based on its name
// If there's no such port, return nullopt
std::optional<Handle> Kernel::getPortHandle(const char* name) {
	for (auto handle : portHandles) {
		const auto data = objects[handle].getData<Port>();
		if (std::strncmp(name, data->name, Port::maxNameLen) == 0) {
			return handle;
		}
	}

	return std::nullopt;
}

// Result ConnectToPort(Handle* out, const char* portName)
void Kernel::connectToPort() {
	const u32 handlePointer = regs[0];
	const u32 portNameUserPtr = regs[1];

	const u32 PortNameSize = Port::maxNameLen + 1;

	u8 portName[PortNameSize];

	int portSizeRead = copyStringFromUser(portName, portNameUserPtr, PortNameSize);

	if (portSizeRead < 0) {
		Helpers::panic("ConnectToPort: Port name pointer was invalid\n");
		regs[0] = Result::OS::InvalidPortName;
		return;
	} else if (portSizeRead == PortNameSize && portName[Port::maxNameLen] != '\0') {
		Helpers::panic("ConnectToPort: Port name too long\n");
		regs[0] = Result::OS::PortNameTooLong;
		return;
	}

	portSizeRead = std::max(portSizeRead, 0);
	std::string port(portName, portName + portSizeRead - 1);

	logSVC("ConnectToPort(handle pointer = %X, port = \"%s\")\n", handlePointer, port.c_str());

	// Try getting a handle to the port
	std::optional<Handle> optionalHandle = getPortHandle(port.c_str());
	if (!optionalHandle.has_value()) [[unlikely]] {
		Helpers::panic("ConnectToPort: Port doesn't exist\n");
		regs[0] = Result::Kernel::NotFound;
		return;
	}

	Handle portHandle = optionalHandle.value();

	const auto portData = objects[portHandle].getData<Port>();
	if (!portData->isPublic) {
		Helpers::panic("ConnectToPort: Attempted to connect to private port");
	}

	// TODO: Actually create session
	Handle sessionHandle = makeSession(portHandle);

	regs[0] = Result::Success;
	regs[1] = sessionHandle;
}

// Result SendSyncRequest(Handle session)
// Send an IPC message to a port (typically "srv:") or a service
void Kernel::sendSyncRequest() {
	const auto handle = regs[0];
	u32 messagePointer = getTLSPointer() + 0x80; // The message is stored starting at TLS+0x80
	logSVC("SendSyncRequest(session handle = %X)\n", handle);

	// The sync request is being sent at a service rather than whatever port, so have the service manager intercept it
	if (KernelHandles::isServiceHandle(handle)) {
		// The service call might cause a reschedule and change threads. Hence, set r0 before executing the service call
		// Because if the service call goes first, we might corrupt the new thread's r0!!
		regs[0] = Result::Success;
		serviceManager.sendCommandToService(messagePointer, handle);
		return;
	}

	// Check if our sync request is targetting a file instead of a service
	bool isFileOperation = getObject(handle, KernelObjectType::File) != nullptr;
	if (isFileOperation) {
		regs[0] = Result::Success; // r0 goes first here too
		handleFileOperation(messagePointer, handle);
		return;
	}

	// Check if our sync request is targetting a directory instead of a service
	bool isDirectoryOperation = getObject(handle, KernelObjectType::Directory) != nullptr;
	if (isDirectoryOperation) {
		regs[0] = Result::Success; // r0 goes first here too
		handleDirectoryOperation(messagePointer, handle);
		return;
	}

	// If we're actually communicating with a port
	const auto session = getObject(handle, KernelObjectType::Session);
	if (session == nullptr) [[unlikely]] {
		Helpers::panic("SendSyncRequest: Invalid handle");
		regs[0] = Result::Kernel::InvalidHandle;
		return;
	}

	const auto sessionData = static_cast<Session*>(session->data);
	const Handle portHandle = sessionData->portHandle;

	if (portHandle == srvHandle) { // Special-case SendSyncRequest targetting the "srv: port"
		regs[0] = Result::Success;
		serviceManager.handleSyncRequest(messagePointer);
	} else if (portHandle == errorPortHandle) { // Special-case "err:f" for juicy logs too
		regs[0] = Result::Success;
		handleErrorSyncRequest(messagePointer);
	} else {
		const auto portData = objects[portHandle].getData<Port>();
		Helpers::panic("SendSyncRequest targetting port %s\n", portData->name);
	}
}