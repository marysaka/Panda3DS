#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

class LCDService {
	Handle handle = KernelHandles::LCD;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, gspLCDLogger)

	// Service commands

public:
	LCDService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};