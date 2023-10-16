function(AddClangTidy target)
	find_program(CLANG-TIDY_PATH clang-tidy REQUIRED)

	set_target_properties(${target}
		PROPERTIES CXX_CLANG_TIDY
		"${CLANG-TIDY_PATH}"
		#"${CLANG-TIDY_PATH};-checks=-*,clang-analyzer-*,cert-*,--warnings-as-errors=*"
		)
	set_target_properties(${target}
		PROPERTIES C_CLANG_TIDY
		"${CLANG-TIDY_PATH};-checks=-*,cert-*"
		#"${CLANG-TIDY_PATH};-checks=-*,clang-analyzer-*,cert-*,--warnings-as-errors=*"
	)
endfunction()
