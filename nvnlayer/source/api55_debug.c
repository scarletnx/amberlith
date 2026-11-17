// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "api55.h"

// The API 55 build's debug calls, answered as a console's release driver answers them, which is
// the driver a game ships with: it installs no callback, walks no database of objects, numbers no
// object, has no debugger attached and records no checkpoint, each of whose calls answers true;
// a label and a marker go nowhere. ScarletNX has those answers from an NVN 55 driver (its
// nvn_object.c), and they are what a program built against the layer gets on every machine.

/// The numbers nvnDeviceGenerateDebugDomainId has given: the next is one more.
static int debug_domains;

void Nvnl_nvnSetExternalDebuggerEnabledNVX(NVNboolean enabled) {
    (void)enabled;
}

NVNdebugDomainId Nvnl_nvnDeviceGenerateDebugDomainId(const NVNdevice *device, const char *name) {
    (void)name;
    (void)DeviceOf(device, "nvnDeviceGenerateDebugDomainId");
    return debug_domains++;
}

void Nvnl_nvnDeviceInstallDebugCallback(NVNdevice *device, PFNNVNDEBUGCALLBACKPROC callback,
                                        void *data, NVNboolean enable) {
    (void)callback;
    (void)data;
    (void)enable;
    (void)DeviceOf(device, "nvnDeviceInstallDebugCallback");
}

NVNboolean Nvnl_nvnDeviceIsExternalDebuggerAttached(const NVNdevice *device) {
    (void)DeviceOf(device, "nvnDeviceIsExternalDebuggerAttached");
    return 0;
}

void Nvnl_nvnDeviceWalkDebugDatabase(const NVNdevice *device, NVNdebugObjectType type,
                                     PFNNVNWALKDEBUGDATABASECALLBACKPROC callback, void *data) {
    (void)type;
    (void)callback;
    (void)data;
    (void)DeviceOf(device, "nvnDeviceWalkDebugDatabase");
}

// The checkpoints' switches: each is taken, and nothing is recorded.
#define CHECKPOINT_SWITCH(name, type)                                                             \
    NVNboolean Nvnl_nvnDeviceSet##name(NVNdevice *device, type value) {                           \
        (void)value;                                                                              \
        return DeviceOf(device, "nvnDeviceSet" #name) != NULL;                                    \
    }
CHECKPOINT_SWITCH(AutomaticCheckpointsByDebugGroup, NVNboolean)
CHECKPOINT_SWITCH(AutomaticCheckpointsEnable, NVNboolean)
CHECKPOINT_SWITCH(AutomaticCheckpointsMask, uint32_t)
CHECKPOINT_SWITCH(AutomaticCheckpointsSamplingInterval, uint32_t)
CHECKPOINT_SWITCH(CheckpointCallStacksEnable, NVNboolean)
CHECKPOINT_SWITCH(CheckpointMaxCallStacksDepth, int)
#undef CHECKPOINT_SWITCH

// A label names an object for the debug layer alone.
#define DEBUG_LABEL(name, type, of)                                                               \
    void Nvnl_nvn##name##SetDebugLabel(type *object, const char *label) {                         \
        (void)label;                                                                              \
        (void)of(object, "nvn" #name "SetDebugLabel");                                            \
    }
DEBUG_LABEL(Device, NVNdevice, DeviceOf)
DEBUG_LABEL(Queue, NVNqueue, QueueOf)
DEBUG_LABEL(MemoryPool, NVNmemoryPool, PoolOf)
DEBUG_LABEL(Texture, NVNtexture, TextureOf)
DEBUG_LABEL(CommandBuffer, NVNcommandBuffer, CmdOf)
DEBUG_LABEL(Window, NVNwindow, WindowOf)
DEBUG_LABEL(Buffer, NVNbuffer, Record)
DEBUG_LABEL(Sampler, NVNsampler, Record)
DEBUG_LABEL(Program, NVNprogram, Record)
DEBUG_LABEL(Sync, NVNsync, Record)
DEBUG_LABEL(TexturePool, NVNtexturePool, Record)
DEBUG_LABEL(SamplerPool, NVNsamplerPool, Record)
#undef DEBUG_LABEL

// Only the debug layer numbers objects.
#define DEBUG_ID(name, type)                                                                      \
    uint64_t Nvnl_nvn##name##GetDebugID(const type *object) {                                     \
        (void)Record(object, "nvn" #name "GetDebugID");                                           \
        return 0u;                                                                                \
    }
DEBUG_ID(Buffer, NVNbuffer)
DEBUG_ID(Texture, NVNtexture)
DEBUG_ID(Sampler, NVNsampler)
#undef DEBUG_ID

// Markers and groups name parts of a list for the debug layer; a list holds none of them.
#define DEBUG_MARKER(name, parameters)                                                            \
    void Nvnl_nvnCommandBuffer##name parameters {                                                 \
        (void)CmdOf(cmd, "nvnCommandBuffer" #name);                                               \
    }
DEBUG_MARKER(InsertDebugMarker, (NVNcommandBuffer *cmd, const char *text))
DEBUG_MARKER(InsertDebugMarkerDynamic,
             (NVNcommandBuffer *cmd, NVNdebugDomainId domain, const char *text))
DEBUG_MARKER(InsertDebugMarkerStatic,
             (NVNcommandBuffer *cmd, NVNdebugDomainId domain, const char *text))
DEBUG_MARKER(PushDebugGroup, (NVNcommandBuffer *cmd, NVNdebugDomainId domain, const char *text))
DEBUG_MARKER(PushDebugGroupDynamic,
             (NVNcommandBuffer *cmd, NVNdebugDomainId domain, const char *text))
DEBUG_MARKER(PushDebugGroupStatic,
             (NVNcommandBuffer *cmd, NVNdebugDomainId domain, const char *text))
DEBUG_MARKER(PopDebugGroup, (NVNcommandBuffer *cmd))
DEBUG_MARKER(PopDebugGroupId, (NVNcommandBuffer *cmd, NVNdebugDomainId domain))
#undef DEBUG_MARKER

NVNboolean Nvnl_nvnCommandBufferInsertCheckpointDynamic(NVNcommandBuffer *cmd,
                                                        const char *label) {
    (void)label;
    return CmdOf(cmd, "nvnCommandBufferInsertCheckpointDynamic") != NULL;
}

NVNboolean Nvnl_nvnCommandBufferInsertCheckpointStatic(NVNcommandBuffer *cmd,
                                                       const char *label) {
    (void)label;
    return CmdOf(cmd, "nvnCommandBufferInsertCheckpointStatic") != NULL;
}
