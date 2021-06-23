cmake_minimum_required (VERSION 3.2)

macro(setup_default_compiler_flags _project_name)
	if(MSVC)
		# Replace some default compiler switches and add new ones
		STRING(REPLACE "/GR" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})			# Disable RTTI
		STRING(REPLACE "/W3" "/W4" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})		# Bump warnings to W4
		target_compile_options(${_project_name} PRIVATE /Zi)				# Add debug info
		target_compile_options(${_project_name} PRIVATE /Oi)				# Generate intrinsic functions
		target_compile_options(${_project_name} PRIVATE /MP)				# Enable parallel compilation

		if(MSVC_VERSION GREATER 1900)
			# VS2017 and above
			target_compile_options(${_project_name} PRIVATE /permissive-)
		endif()

		if(USE_SIMD_INSTRUCTIONS)
			if(USE_AVX_INSTRUCTIONS)
				target_compile_options(${_project_name} PRIVATE "/arch:AVX")
			endif()
		else()
			add_definitions(-DRTM_NO_INTRINSICS)
			add_definitions(-DACL_NO_INTRINSICS)
		endif()

		# Add linker flags
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG")
	endif()
endmacro()
