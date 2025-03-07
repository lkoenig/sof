/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 *
 * \file generic.h
 * \brief Generic Module API header file
 * \author Marcin Rajwa <marcin.rajwa@linux.intel.com>
 *
 */

#ifndef __SOF_AUDIO_MODULE_GENERIC__
#define __SOF_AUDIO_MODULE_GENERIC__

#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/ut.h>
#include <sof/lib/memory.h>

#define module_get_private_data(mod) (mod->priv.private)
#define MAX_BLOB_SIZE 8192

#define API_CALL(cd, cmd, sub_cmd, value, ret) \
	do { \
		ret = (cd)->api((cd)->self, \
				(cmd), \
				(sub_cmd), \
				(value)); \
	} while (0)

#define DECLARE_MODULE_ADAPTER(adapter, uuid, tr) \
static struct comp_dev *module_##adapter##_shim_new(const struct comp_driver *drv, \
					 struct comp_ipc_config *config, \
					 void *spec) \
{ \
	return module_adapter_new(drv, config, &(adapter), spec);\
} \
\
static const struct comp_driver comp_##adapter##_module = { \
	.type = SOF_COMP_MODULE_ADAPTER, \
	.uid = SOF_RT_UUID(uuid), \
	.tctx = &(tr), \
	.ops = { \
		.create = module_##adapter##_shim_new, \
		.prepare = module_adapter_prepare, \
		.params = module_adapter_params, \
		.copy = module_adapter_copy, \
		.cmd = module_adapter_cmd, \
		.trigger = module_adapter_trigger, \
		.reset = module_adapter_reset, \
		.free = module_adapter_free, \
		.set_large_config = module_set_large_config,\
		.get_large_config = module_get_large_config,\
	}, \
}; \
\
static SHARED_DATA struct comp_driver_info comp_module_##adapter##_info = { \
	.drv = &comp_##adapter##_module, \
}; \
\
UT_STATIC void sys_comp_module_##adapter##_init(void) \
{ \
	comp_register(platform_shared_get(&comp_module_##adapter##_info, \
					  sizeof(comp_module_##adapter##_info))); \
} \
\
DECLARE_MODULE(sys_comp_module_##adapter##_init)

struct processing_module;

/**
 * \enum module_cfg_fragment_position
 * \brief Fragment position in config
 * MODULE_CFG_FRAGMENT_FIRST: first fragment of the large configuration
 * MODULE_CFG_FRAGMENT_SINGLE: only fragment of the configuration
 * MODULE_CFG_FRAGMENT_LAST: last fragment of the configuration
 * MODULE_CFG_FRAGMENT_MIDDLE: intermediate fragment of the large configuration
 */
enum module_cfg_fragment_position {
	MODULE_CFG_FRAGMENT_MIDDLE,
	MODULE_CFG_FRAGMENT_FIRST,
	MODULE_CFG_FRAGMENT_LAST,
	MODULE_CFG_FRAGMENT_SINGLE,
};

/**
 * \enum module_processing_mode
 * MODULE_PROCESSING_NORMAL: Indicates that module is expected to apply its custom processing on
 *			      the input signal
 * MODULE_PROCESSING_BYPASS: Indicates that module is expected to skip custom processing on
 *			      the input signal and act as a passthrough component
 */

enum module_processing_mode {
	MODULE_PROCESSING_NORMAL,
	MODULE_PROCESSING_BYPASS,
};

/**
 * \struct input_stream_buffer
 * \brief Input stream buffer
 */
struct input_stream_buffer {
	void __sparse_cache *data; /* data stream buffer */
	uint32_t size; /* size of data in the buffer */
	uint32_t consumed; /* number of bytes consumed by the module */

	/* Indicates end of stream condition has occurred on the input stream */
	bool end_of_stream;
};

/**
 * \struct output_stream_buffer
 * \brief Output stream buffer
 */
struct output_stream_buffer {
	void __sparse_cache *data; /* data stream buffer */
	uint32_t size; /* size of data in the buffer */
};

/*****************************************************************************/
/* Module generic data types						     */
/*****************************************************************************/
/**
 * \struct module_interface
 * \brief 3rd party processing module interface
 */
struct module_interface {
	/**
	 * Module specific initialization procedure, called as part of
	 * module_adapter component creation in .new()
	 */
	int (*init)(struct processing_module *mod);
	/**
	 * Module specific prepare procedure, called as part of module_adapter
	 * component preparation in .prepare()
	 */
	int (*prepare)(struct processing_module *mod);
	/**
	 * Module specific processing procedure, called as part of module_adapter
	 * component copy in .copy(). This procedure is responsible to consume
	 * samples provided by the module_adapter and produce/output the processed
	 * ones back to module_adapter.
	 */
	int (*process)(struct processing_module *mod, struct input_stream_buffer *input_buffers,
		       int num_input_buffers, struct output_stream_buffer *output_buffers,
		       int num_output_buffers);

	/**
	 * Set module configuration for the given configuration ID
	 *
	 * If the complete configuration message is greater than MAX_BLOB_SIZE bytes, the
	 * transmission will be split into several smaller fragments.
	 * In this case the ADSP System will perform multiple calls to SetConfiguration() until
	 * completion of the configuration message sending.
	 * \note config_id indicates ID of the configuration message only on the first fragment
	 * sending, otherwise it is set to 0.
	 */
	int (*set_configuration)(struct processing_module *mod,
				 uint32_t config_id,
				 enum module_cfg_fragment_position pos, uint32_t data_offset_size,
				 const uint8_t *fragment, size_t fragment_size, uint8_t *response,
				 size_t response_size);

	/**
	 * Get module runtime configuration for the given configuration ID
	 *
	 * If the complete configuration message is greater than MAX_BLOB_SIZE bytes, the
	 * transmission will be split into several smaller fragments.
	 * In this case the ADSP System will perform multiple calls to GetConfiguration() until
	 * completion of the configuration message retrieval.
	 * \note config_id indicates ID of the configuration message only on the first fragment
	 * retrieval, otherwise it is set to 0.
	 */
	int (*get_configuration)(struct processing_module *mod,
				 uint32_t config_id, uint32_t *data_offset_size,
				 uint8_t *fragment, size_t fragment_size);

	/**
	 * Set processing mode for the module
	 */
	int (*set_processing_mode)(struct processing_module *mod,
				   enum module_processing_mode mode);

	/**
	 * Get the current processing mode for the module
	 */
	enum module_processing_mode (*get_processing_mode)(struct processing_module *mod);

	/**
	 * Module specific reset procedure, called as part of module_adapter component
	 * reset in .reset(). This should reset all parameters to their initial stage
	 * but leave allocated memory intact.
	 */
	int (*reset)(struct processing_module *mod);
	/**
	 * Module specific free procedure, called as part of module_adapter component
	 * free in .free(). This should free all memory allocated by module.
	 */
	int (*free)(struct processing_module *mod);
};

/**
 * \enum module_state
 * \brief Module-specific states
 */
enum module_state {
	MODULE_DISABLED, /**< Module isn't initialized yet or has been freed.*/
	MODULE_INITIALIZED, /**< Module initialized or reset. */
	MODULE_IDLE, /**< Module is idle now. */
	MODULE_PROCESSING, /**< Module is processing samples now. */
};

/**
 * \struct module_param
 * \brief Module TLV parameters container - used for both config types.
 * For example if one want to set the sample_rate to 16 [kHz] and this
 * parameter was assigned to id 0x01, its max size is four bytes then the
 * configuration filed should look like this (note little-endian format):
 * 0x01 0x00 0x00 0x00, 0x0C 0x00 0x00 0x00, 0x10 0x00 0x00 0x00.
 */
struct module_param {
	/**
	 * Specifies the unique id of a parameter. For example the parameter
	 * sample_rate may have an id of 0x01.
	 */
	uint32_t id;
	uint32_t size; /**< The size of whole parameter - id + size + data */
	int32_t data[]; /**< A pointer to memory where config is stored.*/
};

/**
 * \struct module_config
 * \brief Module config container, used for both config types.
 */
struct module_config {
	size_t size; /**< Specifies the size of whole config */
	bool avail; /**< Marks config as available to use.*/
	void *data; /**< tlv config, a pointer to memory where config is stored. */
};

/**
 * \struct module_memory
 * \brief module memory block - used for every memory allocated by module
 */
struct module_memory {
	void *ptr; /**< A pointr to particular memory block */
	struct list_item mem_list; /**< list of memory allocated by module */
};

/**
 * \struct module_processing_data
 * \brief Processing data shared between particular module & module_adapter
 */
struct module_processing_data {
	uint32_t in_buff_size; /**< Specifies the size of module input buffer. */
	uint32_t out_buff_size; /**< Specifies the size of module output buffer.*/
	uint32_t avail; /**< Specifies how much data is available for module to process.*/
	uint32_t produced; /**< Specifies how much data the module produced in its last task.*/
	uint32_t consumed; /**< Specified how much data the module consumed in its last task */
	uint32_t init_done; /**< Specifies if the module initialization is finished */
	void *in_buff; /**< A pointer to module input buffer. */
	void *out_buff; /**< A pointer to module output buffer. */
};

/** private, runtime module data */
struct module_data {
	enum module_state state;
	size_t new_cfg_size; /**< size of new module config data */
	void *private; /**< self object, memory tables etc here */
	void *runtime_params;
	struct module_config cfg; /**< module configuration data */
	struct module_interface *ops; /**< module specific operations */
	struct module_memory memory; /**< memory allocated by module */
	struct module_processing_data mpd; /**< shared data comp <-> module */
};

/* module_adapter private, runtime data */
struct processing_module {
	struct module_data priv; /**< module private data */
	struct sof_ipc_stream_params *stream_params;
	struct list_item sink_buffer_list; /* list of sink buffers to save produced output */

	/*
	 * This is a temporary change in order to support the trace messages in the modules. This
	 * will be removed once the trace API is updated.
	 */
	struct comp_dev *dev;
	uint32_t period_bytes; /** pipeline period bytes */
	uint32_t deep_buff_bytes; /**< copy start threshold */
	uint32_t output_buffer_size; /**< size of local buffer to save produced samples */
	struct input_stream_buffer *input_buffers;
	struct output_stream_buffer *output_buffers;
	uint32_t num_input_buffers; /**< number of input buffers */
	uint32_t num_output_buffers; /**< number of output buffers */
	/*
	 * flag set by a module when it has 1 input buffer and 1 output buffer and produces
	 * period_bytes every copy
	 */
	bool simple_copy;
};

/*****************************************************************************/
/* Module generic interfaces						     */
/*****************************************************************************/
int module_load_config(struct comp_dev *dev, void *cfg, size_t size);
int module_init(struct processing_module *mod, struct module_interface *interface);
void *module_allocate_memory(struct processing_module *mod, uint32_t size, uint32_t alignment);
int module_free_memory(struct processing_module *mod, void *ptr);
void module_free_all_memory(struct processing_module *mod);
int module_prepare(struct processing_module *mod);
int module_process(struct processing_module *mod, struct input_stream_buffer *input_buffers,
		   int num_input_buffers, struct output_stream_buffer *output_buffers,
		   int num_output_buffers);
int module_reset(struct processing_module *mod);
int module_free(struct processing_module *mod);
int module_set_configuration(struct processing_module *mod,
			     uint32_t config_id,
			     enum module_cfg_fragment_position pos, size_t data_offset_size,
			     const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			     size_t response_size);

struct comp_dev *module_adapter_new(const struct comp_driver *drv,
				    struct comp_ipc_config *config,
				    struct module_interface *interface, void *spec);
int module_adapter_prepare(struct comp_dev *dev);
int module_adapter_params(struct comp_dev *dev, struct sof_ipc_stream_params *params);
int module_adapter_copy(struct comp_dev *dev);
int module_adapter_cmd(struct comp_dev *dev, int cmd, void *data, int max_data_size);
int module_adapter_trigger(struct comp_dev *dev, int cmd);
void module_adapter_free(struct comp_dev *dev);
int module_adapter_reset(struct comp_dev *dev);
int module_set_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
			    bool last_block, uint32_t data_offset, char *data);
int module_get_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
			    bool last_block, uint32_t *data_offset, char *data);

#endif /* __SOF_AUDIO_MODULE_GENERIC__ */
