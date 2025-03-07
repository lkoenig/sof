# This is still WIP - Not fully validated on any platform.

# When west is installed, Zephyr's CMake invokes west to list and try to
# compile every Zephyr module that can be found.
if(CONFIG_SOF)

if(CONFIG_LIBRARY)
	set(PLATFORM "library")
	set(ARCH host)
	zephyr_include_directories(../src/platform/library/include)
else()
	# firmware build supports only xtensa arch for now
	set(ARCH xtensa)
endif()

# Appends literal with path of the source file relative to the project root
# It is useful if sources in given target need deterministic relative path
# to the actually compiled file.
# __FILE is not always suitable as C standard states that __FILE__ expands to
# input file name, that usually is absolute path what will cause f.e. .rodata
# size to be dependent on where project is physically located on the disk.
function(sof_append_relative_path_definitions target)
	get_target_property(sources ${target} SOURCES)
	foreach(src ${sources})
		get_filename_component(ABS_PATH ${src} ABSOLUTE)
		file(RELATIVE_PATH rel ${PROJECT_SOURCE_DIR} ${ABS_PATH})
		set_property(
			SOURCE ${src}
			APPEND
			PROPERTY COMPILE_DEFINITIONS
			RELATIVE_FILE="${rel}")
	endforeach()
endfunction()

# Initial SOF module will contain
#
# 1. Application logic - pipeline, audio components, IPC processing, topology
# 2. IP drivers - SSP, DMIC, PM, IPC will transition to Zephyr directly over
#                 time and be removed from the SOF repo.
# 3. Platform IP - PM, init, clocks, IRQs will transition directly to Zephyr
#                  over time and be removed from SOF repo.
# 4. RTOS logic - scheduler, allocator, notifier - as with 2 & 3.
zephyr_interface_library_named(SOF)

# SOF source paths.
set(SOF_SRC_PATH "../src")
set(SOF_PLATFORM_PATH "${SOF_SRC_PATH}/platform")
set(SOF_AUDIO_PATH "${SOF_SRC_PATH}/audio")
set(SOF_AUDIO_MODULES_PATH "${SOF_SRC_PATH}/audio/module_adapter/module")
set(SOF_SAMPLES_PATH "${SOF_SRC_PATH}/samples")
set(SOF_LIB_PATH "${SOF_SRC_PATH}/lib")
set(SOF_DRIVERS_PATH "${SOF_SRC_PATH}/drivers")
set(SOF_IPC_PATH "${SOF_SRC_PATH}/ipc")
set(SOF_DEBUG_PATH "${SOF_SRC_PATH}/debug")
set(SOF_MATH_PATH "${SOF_SRC_PATH}/math")
set(SOF_TRACE_PATH "${SOF_SRC_PATH}/trace")

# Save path to rimage configuration files in cmake cache for later use by
# rimage during the "west sign" stage
get_filename_component(RIMAGE_CONFIG "../rimage/config" ABSOLUTE)
set(RIMAGE_CONFIG_PATH ${RIMAGE_CONFIG} CACHE PATH
    " Path to rimage board configuration files")

include(ExternalProject)

ExternalProject_Add(smex_ep
	SOURCE_DIR "${ZEPHYR_SOF_MODULE_DIR}/smex/"
	# The default paths are very "deep"
	PREFIX     "${PROJECT_BINARY_DIR}/smex_ep"
	BINARY_DIR "${PROJECT_BINARY_DIR}/smex_ep/build"
	BUILD_ALWAYS 1
	INSTALL_COMMAND "" # need smex only at build time
)

ExternalProject_Add(sof_logger_ep
	SOURCE_DIR "${ZEPHYR_SOF_MODULE_DIR}/tools/"
	# The default paths are very "deep"
	PREFIX     "${PROJECT_BINARY_DIR}/sof-logger_ep"
	BINARY_DIR "${PROJECT_BINARY_DIR}/sof-logger_ep/build"
	BUILD_COMMAND cmake --build . --target sof-logger
	BUILD_ALWAYS 1
	INSTALL_COMMAND ""
)

# default SOF includes
target_include_directories(SOF INTERFACE ../rimage/src/include)
target_include_directories(SOF INTERFACE ../zephyr/include)
target_include_directories(SOF INTERFACE ${SOF_SRC_PATH}/include)
target_include_directories(SOF INTERFACE ${SOF_SRC_PATH}/arch/${ARCH}/include)

# SOF module init
zephyr_library_named(modules_sof)
zephyr_include_directories(
	include
)

# SOC level sources
# Files that are commented may not be needed.

# Intel BYT, CHT, BSW platforms
if (CONFIG_SOC_SERIES_INTEL_ADSP_BAYTRAIL)

	# Driver sources
	zephyr_library_sources(
		${SOF_DRIVERS_PATH}/intel/baytrail/ipc.c
		${SOF_DRIVERS_PATH}/intel/baytrail/ssp.c
		${SOF_DRIVERS_PATH}/intel/pmc-ipc.c
	)

	# Platform sources
	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/baytrail/platform.c
		${SOF_PLATFORM_PATH}/baytrail/lib/dai.c
		${SOF_PLATFORM_PATH}/baytrail/lib/clk.c
		${SOF_PLATFORM_PATH}/baytrail/lib/dma.c
		${SOF_SRC_PATH}/schedule/dma_multi_chan_domain.c
	)

	# SOF core infrastructure - runs on top of Zephyr
	zephyr_library_sources(
		${SOF_SRC_PATH}/schedule/ll_schedule.c
	)

	set(PLATFORM "baytrail")
endif()

# Intel HSW, BDW platforms
if (CONFIG_SOC_SERIES_INTEL_ADSP_BROADWELL)
	zephyr_library_sources(
		${SOF_DRIVERS_PATH}/intel/haswell/ipc.c
		${SOF_DRIVERS_PATH}/intel/haswell/ssp.c
	)

	# SOF core infrastructure - runs on top of Zephyr
	zephyr_library_sources(
		${SOF_SRC_PATH}/schedule/ll_schedule.c
	)

	set(PLATFORM "haswell")
endif()

# Intel APL, KBL, SKL CAVS 1.5 platforms
if (CONFIG_SOC_SERIES_INTEL_CAVS_V15)

	# Driver sources
	zephyr_library_sources(
		${SOF_DRIVERS_PATH}/intel/cavs/ipc.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_HDA
		${SOF_DRIVERS_PATH}/intel/hda/hda-dma.c
	)

	if (NOT CONFIG_ZEPHYR_NATIVE_DRIVERS)
	zephyr_library_sources(
		${SOF_DRIVERS_PATH}/intel/cavs/timestamp.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_HDA
		${SOF_DRIVERS_PATH}/intel/hda/hda.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_MN
		${SOF_DRIVERS_PATH}/intel/ssp/mn.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_SSP
		${SOF_DRIVERS_PATH}/intel/ssp/ssp.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_ALH
		${SOF_DRIVERS_PATH}/intel/alh.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_DMIC
		${SOF_DRIVERS_PATH}/intel/dmic/dmic.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_DMIC_TPLG_PARAMS
		${SOF_DRIVERS_PATH}/intel/dmic/dmic_computed.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_DMIC_NHLT
		${SOF_DRIVERS_PATH}/intel/dmic/dmic_nhlt.c
	)

	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/intel/cavs/lib/dai.c
	)
	endif()

	# Platform sources
	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/intel/cavs/platform.c
		${SOF_PLATFORM_PATH}/intel/cavs/lib/mem_window.c
		${SOF_PLATFORM_PATH}/intel/cavs/lib/pm_runtime.c
		${SOF_PLATFORM_PATH}/intel/cavs/lib/pm_memory.c
		${SOF_PLATFORM_PATH}/intel/cavs/lib/clk.c
		${SOF_PLATFORM_PATH}/intel/cavs/lib/dma.c
		${SOF_PLATFORM_PATH}/apollolake/lib/power_down.S
		${SOF_PLATFORM_PATH}/apollolake/lib/clk.c
	)

	# SOF core infrastructure - runs on top of Zephyr
	zephyr_library_sources(
		${SOF_SRC_PATH}/schedule/zephyr_ll.c
	)

	set_source_files_properties(${SOF_PLATFORM_PATH}/apollolake/lib/power_down.S PROPERTIES COMPILE_FLAGS -DASSEMBLY)

	set(PLATFORM "apollolake")
	zephyr_include_directories(${SOF_PLATFORM_PATH}/intel/cavs/include)
endif()

# Intel CNL and CAVS 1.8 platfroms
if (CONFIG_SOC_SERIES_INTEL_CAVS_V18)

	# Driver sources
	zephyr_library_sources(
		${SOF_DRIVERS_PATH}/intel/cavs/ipc.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_HDA
		${SOF_DRIVERS_PATH}/intel/hda/hda-dma.c
	)

	if (NOT CONFIG_ZEPHYR_NATIVE_DRIVERS)
	zephyr_library_sources(
		${SOF_DRIVERS_PATH}/intel/cavs/timestamp.c
	)

	# Sue Creek - S100 only - already in Zephyr.
	#${SOF_DRIVERS_PATH}/intel/cavs/sue-ipc.c
	#${SOF_DRIVERS_PATH}/intel/cavs/sue-iomux.c

	zephyr_library_sources_ifdef(CONFIG_INTEL_HDA
		${SOF_DRIVERS_PATH}/intel/hda/hda.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_MN
		${SOF_DRIVERS_PATH}/intel/ssp/mn.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_SSP
		${SOF_DRIVERS_PATH}/intel/ssp/ssp.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_ALH
		${SOF_DRIVERS_PATH}/intel/alh.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_DMIC
		${SOF_DRIVERS_PATH}/intel/dmic/dmic.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_DMIC_TPLG_PARAMS
		${SOF_DRIVERS_PATH}/intel/dmic/dmic_computed.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_DMIC_NHLT
		${SOF_DRIVERS_PATH}/intel/dmic/dmic_nhlt.c
	)

	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/intel/cavs/lib/dai.c
	)
	endif()

	# Platform sources
	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/intel/cavs/platform.c
		${SOF_PLATFORM_PATH}/intel/cavs/lib/mem_window.c
		${SOF_PLATFORM_PATH}/cannonlake/lib/clk.c
		${SOF_PLATFORM_PATH}/intel/cavs/lib/power_down.S
		${SOF_PLATFORM_PATH}/intel/cavs/lib/pm_runtime.c
		${SOF_PLATFORM_PATH}/intel/cavs/lib/pm_memory.c
		${SOF_PLATFORM_PATH}/intel/cavs/lib/clk.c
		${SOF_PLATFORM_PATH}/intel/cavs/lib/dma.c
		#${SOF_PLATFORM_PATH}/intel/cavs/lps_pic_restore_vector.S
	)

	# SOF core infrastructure - runs on top of Zephyr
	zephyr_library_sources(
		${SOF_SRC_PATH}/schedule/zephyr_ll.c
	)

	zephyr_library_sources_ifdef(CONFIG_CAVS_LPS
		${SOF_PLATFORM_PATH}/intel/cavs/lps_wait.c
	)

	set_source_files_properties(${SOF_PLATFORM_PATH}/intel/cavs/lib/power_down.S PROPERTIES COMPILE_FLAGS -DASSEMBLY)
	set_source_files_properties(${SOF_PLATFORM_PATH}/intel/cavs/lps_pic_restore_vector.S PROPERTIES COMPILE_FLAGS -DASSEMBLY)

	set(PLATFORM "cannonlake")
	zephyr_include_directories(${SOF_PLATFORM_PATH}/intel/cavs/include)
endif()

# Intel ICL and CAVS 2.0 platforms
if (CONFIG_SOC_SERIES_INTEL_CAVS_V20)

	# Driver sources
	zephyr_library_sources(
		${SOF_DRIVERS_PATH}/intel/cavs/ipc.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_HDA
		${SOF_DRIVERS_PATH}/intel/hda/hda-dma.c
	)

	if (NOT CONFIG_ZEPHYR_NATIVE_DRIVERS)
	zephyr_library_sources(
		${SOF_DRIVERS_PATH}/intel/cavs/timestamp.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_HDA
		${SOF_DRIVERS_PATH}/intel/hda/hda.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_MN
		${SOF_DRIVERS_PATH}/intel/ssp/mn.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_SSP
		${SOF_DRIVERS_PATH}/intel/ssp/ssp.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_ALH
		${SOF_DRIVERS_PATH}/intel/alh.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_DMIC
		${SOF_DRIVERS_PATH}/intel/dmic/dmic.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_DMIC_TPLG_PARAMS
		${SOF_DRIVERS_PATH}/intel/dmic/dmic_computed.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_DMIC_NHLT
		${SOF_DRIVERS_PATH}/intel/dmic/dmic_nhlt.c
	)

	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/intel/cavs/lib/dai.c
	)
	endif()

	# Platform sources
	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/intel/cavs/platform.c
		${SOF_PLATFORM_PATH}/intel/cavs/lib/mem_window.c
		${SOF_PLATFORM_PATH}/icelake/lib/clk.c
		${SOF_PLATFORM_PATH}/intel/cavs/lib/power_down.S
		${SOF_PLATFORM_PATH}/intel/cavs/lib/pm_runtime.c
		${SOF_PLATFORM_PATH}/intel/cavs/lib/pm_memory.c
		${SOF_PLATFORM_PATH}/intel/cavs/lib/clk.c
		${SOF_PLATFORM_PATH}/intel/cavs/lib/dma.c
		#${SOF_PLATFORM_PATH}/intel/cavs/lps_pic_restore_vector.S
	)

	# SOF core infrastructure - runs on top of Zephyr
	zephyr_library_sources(
		${SOF_SRC_PATH}/schedule/zephyr_ll.c
	)

	zephyr_library_sources_ifdef(CONFIG_CAVS_LPS
		${SOF_PLATFORM_PATH}/intel/cavs/lps_wait.c
	)

	set_source_files_properties(${SOF_PLATFORM_PATH}/intel/cavs/lib/power_down.S PROPERTIES COMPILE_FLAGS -DASSEMBLY)
	set_source_files_properties(${SOF_PLATFORM_PATH}/intel/cavs/lps_pic_restore_vector.S PROPERTIES COMPILE_FLAGS -DASSEMBLY)

	set(PLATFORM "icelake")
	zephyr_include_directories(${SOF_PLATFORM_PATH}/intel/cavs/include)
endif()

# Intel TGL and CAVS 2.5 platforms
if (CONFIG_SOC_SERIES_INTEL_CAVS_V25)

	# Driver sources
	zephyr_library_sources(
		${SOF_DRIVERS_PATH}/intel/cavs/ipc.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_HDA
		${SOF_DRIVERS_PATH}/intel/hda/hda-dma.c
	)

	if (NOT CONFIG_ZEPHYR_NATIVE_DRIVERS)
	zephyr_library_sources(
		${SOF_DRIVERS_PATH}/intel/cavs/timestamp.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_HDA
		${SOF_DRIVERS_PATH}/intel/hda/hda.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_MN
		${SOF_DRIVERS_PATH}/intel/ssp/mn.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_SSP
		${SOF_DRIVERS_PATH}/intel/ssp/ssp.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_ALH
		${SOF_DRIVERS_PATH}/intel/alh.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_DMIC
		${SOF_DRIVERS_PATH}/intel/dmic/dmic.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_DMIC_TPLG_PARAMS
		${SOF_DRIVERS_PATH}/intel/dmic/dmic_computed.c
	)

	zephyr_library_sources_ifdef(CONFIG_INTEL_DMIC_NHLT
		${SOF_DRIVERS_PATH}/intel/dmic/dmic_nhlt.c
	)

	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/intel/cavs/lib/dai.c
	)
	endif()

	# Platform sources
	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/intel/cavs/platform.c
		${SOF_PLATFORM_PATH}/intel/cavs/lib/mem_window.c
		${SOF_PLATFORM_PATH}/tigerlake/lib/clk.c
		${SOF_PLATFORM_PATH}/intel/cavs/lib/power_down.S
		${SOF_PLATFORM_PATH}/intel/cavs/lib/pm_runtime.c
		${SOF_PLATFORM_PATH}/intel/cavs/lib/pm_memory.c
		${SOF_PLATFORM_PATH}/intel/cavs/lib/clk.c
		${SOF_PLATFORM_PATH}/intel/cavs/lib/dma.c
		#${SOF_PLATFORM_PATH}/intel/cavs/lps_pic_restore_vector.S
	)

	# SOF core infrastructure - runs on top of Zephyr
	zephyr_library_sources(
		${SOF_SRC_PATH}/schedule/zephyr_ll.c
	)

	zephyr_library_sources_ifdef(CONFIG_CAVS_LPS
		${SOF_PLATFORM_PATH}/intel/cavs/lps_wait.c
	)

	set_source_files_properties(${SOF_PLATFORM_PATH}/intel/cavs/lib/power_down.S PROPERTIES COMPILE_FLAGS -DASSEMBLY)
	set_source_files_properties(${SOF_PLATFORM_PATH}/intel/cavs/lps_pic_restore_vector.S PROPERTIES COMPILE_FLAGS -DASSEMBLY)

	set(PLATFORM "tigerlake")
	zephyr_include_directories(${SOF_PLATFORM_PATH}/intel/cavs/include)
endif()

# NXP IMX8 platforms
if (CONFIG_SOC_SERIES_NXP_IMX8)
	zephyr_library_sources(
		${SOF_DRIVERS_PATH}/generic/dummy-dma.c
		${SOF_DRIVERS_PATH}/imx/edma.c
		${SOF_DRIVERS_PATH}/imx/sai.c
		${SOF_DRIVERS_PATH}/imx/ipc.c
		${SOF_DRIVERS_PATH}/imx/esai.c
		${SOF_DRIVERS_PATH}/imx/interrupt-irqsteer.c
	)

	# Platform sources
	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/imx8/platform.c
		${SOF_PLATFORM_PATH}/imx8/lib/clk.c
		${SOF_PLATFORM_PATH}/imx8/lib/dai.c
		${SOF_PLATFORM_PATH}/imx8/lib/dma.c
		${SOF_PLATFORM_PATH}/imx8/lib/memory.c
	)

	# SOF core infrastructure - runs on top of Zephyr
	zephyr_library_sources(
		${SOF_SRC_PATH}/schedule/ll_schedule.c
		${SOF_SRC_PATH}/drivers/interrupt.c
	)

	set(PLATFORM "imx8")
endif()

if (CONFIG_SOC_SERIES_NXP_IMX8M)
	zephyr_library_sources(
		${SOF_DRIVERS_PATH}/generic/dummy-dma.c
		${SOF_DRIVERS_PATH}/imx/sdma.c
		${SOF_DRIVERS_PATH}/imx/sai.c
		${SOF_DRIVERS_PATH}/imx/ipc.c
		${SOF_DRIVERS_PATH}/imx/interrupt-irqsteer.c
	)

	# Platform sources
	zephyr_library_sources(
		${SOF_PLATFORM_PATH}/imx8m/platform.c
		${SOF_PLATFORM_PATH}/imx8m/lib/clk.c
		${SOF_PLATFORM_PATH}/imx8m/lib/memory.c
		${SOF_PLATFORM_PATH}/imx8m/lib/dai.c
		${SOF_PLATFORM_PATH}/imx8m/lib/dma.c
	)

	# SOF core infrastructure - runs on top of Zephyr
	zephyr_library_sources(
		${SOF_SRC_PATH}/schedule/ll_schedule.c
		${SOF_SRC_PATH}/drivers/interrupt.c
	)

	set(PLATFORM "imx8m")
endif()

zephyr_library_sources_ifdef(CONFIG_LIBRARY
	${SOF_PLATFORM_PATH}/library/platform.c
	${SOF_PLATFORM_PATH}/library/lib/dai.c
	${SOF_DRIVERS_PATH}/host/ipc.c
)

zephyr_include_directories(${SOF_PLATFORM_PATH}/${PLATFORM}/include)

# Mandatory Files used on all platforms.
# Commented files will be added/removed as integration dictates.
zephyr_library_sources(
	${SOF_IPC_PATH}/dma-copy.c
	${SOF_IPC_PATH}/ipc-common.c
	${SOF_IPC_PATH}/ipc-helper.c
	${SOF_SRC_PATH}/spinlock.c

	# SOF math utilities
	${SOF_MATH_PATH}/decibels.c
	${SOF_MATH_PATH}/numbers.c
	${SOF_MATH_PATH}/trig.c

	# SOF library - parts to transition to Zephyr over time
	${SOF_LIB_PATH}/clk.c
	${SOF_LIB_PATH}/notifier.c
	${SOF_LIB_PATH}/lib.c
	${SOF_LIB_PATH}/pm_runtime.c
	${SOF_LIB_PATH}/wait.c
	${SOF_LIB_PATH}/dma.c
	${SOF_LIB_PATH}/dai.c

	# SOF mandatory audio processing
	${SOF_AUDIO_PATH}/channel_map.c
	${SOF_AUDIO_PATH}/pcm_converter/pcm_converter_hifi3.c
	${SOF_AUDIO_PATH}/pcm_converter/pcm_converter.c
	${SOF_AUDIO_PATH}/pcm_converter/pcm_converter_generic.c
	${SOF_AUDIO_PATH}/buffer.c
	${SOF_AUDIO_PATH}/component.c
	${SOF_AUDIO_PATH}/pipeline/pipeline-graph.c
	${SOF_AUDIO_PATH}/pipeline/pipeline-params.c
	${SOF_AUDIO_PATH}/pipeline/pipeline-schedule.c
	${SOF_AUDIO_PATH}/pipeline/pipeline-stream.c
	${SOF_AUDIO_PATH}/pipeline/pipeline-xrun.c

	# SOF core infrastructure - runs on top of Zephyr
	${SOF_SRC_PATH}/init/init.c
	${SOF_SRC_PATH}/init/ext_manifest.c
	${SOF_SRC_PATH}/arch/xtensa/drivers/cache_attr.c
	${SOF_SRC_PATH}/schedule/zephyr_domain.c
	${SOF_SRC_PATH}/schedule/schedule.c
	${SOF_SRC_PATH}/schedule/dma_single_chan_domain.c
	${SOF_SRC_PATH}/schedule/dma_multi_chan_domain.c
	${SOF_SRC_PATH}/idc/zephyr_idc.c

	# Bridge wrapper between SOF and Zephyr APIs - Will shrink over time.
	wrapper.c
	edf_schedule.c
	schedule.c
)

if(CONFIG_COMP_BLOB)
	zephyr_library_sources(
		${SOF_AUDIO_PATH}/data_blob.c
	)
endif()

if(CONFIG_ZEPHYR_NATIVE_DRIVERS)
	zephyr_library_sources(
		${SOF_AUDIO_PATH}/host-zephyr.c
	)
else()
	zephyr_library_sources(
		${SOF_AUDIO_PATH}/host-legacy.c
	)
endif()

zephyr_library_sources_ifdef(CONFIG_IPC_MAJOR_3
	${SOF_IPC_PATH}/ipc3/handler.c
	${SOF_IPC_PATH}/ipc3/helper.c
	${SOF_IPC_PATH}/ipc3/dai.c
	${SOF_IPC_PATH}/ipc3/host-page-table.c
)

zephyr_library_sources_ifdef(CONFIG_IPC_MAJOR_4
	${SOF_IPC_PATH}/ipc4/handler.c
	${SOF_IPC_PATH}/ipc4/helper.c
	${SOF_IPC_PATH}/ipc4/dai.c
)

zephyr_library_sources_ifdef(CONFIG_TRACE
	${SOF_SRC_PATH}/trace/dma-trace.c
	${SOF_SRC_PATH}/trace/trace.c
)

# Optional SOF sources - depends on Kconfig - WIP

zephyr_library_sources_ifdef(CONFIG_COMP_FIR
	${SOF_AUDIO_PATH}/eq_fir/eq_fir_hifi3.c
	${SOF_AUDIO_PATH}/eq_fir/eq_fir_hifi2ep.c
	${SOF_AUDIO_PATH}/eq_fir/eq_fir_generic.c
	${SOF_AUDIO_PATH}/eq_fir/eq_fir.c
	${SOF_MATH_PATH}/fir_generic.c
	${SOF_MATH_PATH}/fir_hifi2ep.c
	${SOF_MATH_PATH}/fir_hifi3.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_IIR
	${SOF_MATH_PATH}/iir_df2t_generic.c
	${SOF_MATH_PATH}/iir_df2t_hifi3.c
	${SOF_MATH_PATH}/iir.c
	${SOF_AUDIO_PATH}/eq_iir/eq_iir.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_ASRC
	${SOF_AUDIO_PATH}/asrc/asrc.c
	${SOF_AUDIO_PATH}/asrc/asrc_farrow_hifi3.c
	${SOF_AUDIO_PATH}/asrc/asrc_farrow.c
	${SOF_AUDIO_PATH}/asrc/asrc_farrow_generic.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_DCBLOCK
	${SOF_AUDIO_PATH}/dcblock/dcblock_generic.c
	${SOF_AUDIO_PATH}/dcblock/dcblock.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_SEL
	${SOF_AUDIO_PATH}/selector/selector_generic.c
	${SOF_AUDIO_PATH}/selector/selector.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_KPB
	${SOF_AUDIO_PATH}/kpb.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_SWITCH
	${SOF_AUDIO_PATH}/switch.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_MIXER
	${SOF_AUDIO_PATH}/mixer.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_TONE
	${SOF_AUDIO_PATH}/tone.c
)

if(CONFIG_ZEPHYR_NATIVE_DRIVERS)
	zephyr_library_sources_ifdef(CONFIG_COMP_DAI
		${SOF_AUDIO_PATH}/dai-zephyr.c
)
else()
	zephyr_library_sources_ifdef(CONFIG_COMP_DAI
		${SOF_AUDIO_PATH}/dai-legacy.c
)
endif()

zephyr_library_sources_ifdef(CONFIG_SAMPLE_KEYPHRASE
	${SOF_SAMPLES_PATH}/audio/detect_test.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_VOLUME
	${SOF_AUDIO_MODULES_PATH}/volume/volume_hifi3.c
	${SOF_AUDIO_MODULES_PATH}/volume/volume_generic.c
	${SOF_AUDIO_MODULES_PATH}/volume/volume.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_MODULE_ADAPTER
	${SOF_AUDIO_PATH}/module_adapter/module_adapter.c
	${SOF_AUDIO_PATH}/module_adapter/module/generic.c
)

if (CONFIG_COMP_MODULE_ADAPTER)
zephyr_library_sources_ifdef(CONFIG_CADENCE_CODEC
	${SOF_AUDIO_PATH}/module_adapter/module/cadence.c
)

if (CONFIG_CADENCE_CODEC_MP3_DEC)
zephyr_library_import(xa_mp3_dec ${CONFIG_CADENCE_CODEC_MP3_DEC_LIB})
endif()

if (CONFIG_CADENCE_CODEC_AAC_DEC)
zephyr_library_import(xa_aac_dec ${CONFIG_CADENCE_CODEC_AAC_DEC_LIB})
endif()

zephyr_library_sources_ifdef(CONFIG_PASSTHROUGH_CODEC
	${SOF_AUDIO_MODULES_PATH}/passthrough.c
)
endif()

zephyr_library_sources_ifdef(CONFIG_COMP_SRC
	${SOF_AUDIO_PATH}/src/src_hifi2ep.c
	${SOF_AUDIO_PATH}/src/src_generic.c
	${SOF_AUDIO_PATH}/src/src_hifi3.c
	${SOF_AUDIO_PATH}/src/src.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_BASEFW_IPC4
	${SOF_AUDIO_PATH}/base_fw.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_COPIER
	${SOF_AUDIO_PATH}/copier.c
)

zephyr_library_sources_ifdef(CONFIG_SAMPLE_SMART_AMP
	${SOF_SAMPLES_PATH}/audio/smart_amp_test.c
)

zephyr_library_sources_ifdef(CONFIG_COMP_MUX
	${SOF_AUDIO_PATH}/mux/mux.c
	${SOF_AUDIO_PATH}/mux/mux_generic.c
)

zephyr_library_sources_ifdef(CONFIG_PROBE
	${SOF_SRC_PATH}/probe/probe.c
)

zephyr_library_sources_ifdef(CONFIG_MULTICORE
	${SOF_SRC_PATH}/idc/idc.c
)

zephyr_library_sources_ifdef(CONFIG_HAVE_AGENT
	${SOF_LIB_PATH}/agent.c
)

zephyr_library_sources_ifdef(CONFIG_GDB_DEBUG
	${SOF_DEBUG_PATH}/gdb/gdb.c
	${SOF_DEBUG_PATH}/gdb/ringbuffer.c
)

zephyr_library_sources_ifdef(CONFIG_DW_DMA
	${SOF_DRIVERS_PATH}/dw/dma.c
)

zephyr_library_link_libraries(SOF)
target_link_libraries(SOF INTERFACE zephyr_interface)

# Setup SOF directories
set(SOF_ROOT_SOURCE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(SOF_ROOT_BINARY_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# This generated/ directory is shared with Zephyr.
# PROJECT_BINARY_DIR is build/zephyr/
set(GENERATED_DIRECTORY ${PROJECT_BINARY_DIR}/include/generated)

set(VERSION_H_PATH ${GENERATED_DIRECTORY}/sof_versions.h)

find_package(Python3 COMPONENTS Interpreter)
set(PYTHON3 "${Python3_EXECUTABLE}")

# SOF uses GNU C99 extensions. TODO other flags required ?
target_compile_options(SOF INTERFACE -std=gnu99 -fno-inline-functions)

# Toolchain info
add_definitions(-DXCC_TOOLS_VERSION="${ZEPHYR_TOOLCHAIN_VARIANT}" -DCC_OPTIMIZE_FLAGS="${OPTIMIZATION_FLAG}")

# create version information
include(../scripts/cmake/version.cmake)

# Create Trace realtive file paths
sof_append_relative_path_definitions(modules_sof)

endif() # CONFIG_SOF
