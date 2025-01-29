
find_program(CLANG-TIDY_PATH clang-tidy REQUIRED)

function(AddClangTidy target)

  set_target_properties(${target}
    PROPERTIES C_CLANG_TIDY
		"${CLANG-TIDY_PATH};-checks=misc-redundant-expression,\
misc-confusable-identifiers,\
readability-uppercase-literal-suffix,\
cert-dcl16-c,\
misc-definitions-in-headers,\
readability-redundant-declaration,\
readability-implicit-bool-conversion,\
bugprone-narrowing-conversions,\
bugprone-integer-division,\
cppcoreguidelines-pro-bounds-pointer-arithmetic,\
#hicpp-signed-bitwise,\
cppcoreguidelines-pro-type-static-cast-downcast,\
cppcoreguidelines-pro-type-const-cast,\
performance-no-int-to-ptr,\
fuchsia-overloaded-operator,\
hicpp-no-array-decay,\
modernize-use-bool-literals,\
google-runtime-operator,\
bugprone-assignment-in-if-condition,\
readability-braces-around-statements,\
hicpp-multiway-paths-covered,\
cert-flp30-c,\
altera-id-dependent-backward-branch,\
readability-non-const-parameter,\
cert-dcl59-cpp,\
google-build-using-namespace,\
google-global-names-in-headers,\
misc-no-recursion,\
#readability-isolate-declaration,\
readability-inconsistent-declaration-parameter-name,\
readability-make-member-function-const,\
readability-convert-member-functions-to-static,\
cppcoreguidelines-pro-type-union-access,\
fuchsia-virtual-inheritance,\
google-explicit-constructor,\
cppcoreguidelines-macro-usage,\
cppcoreguidelines-interfaces-global-init,\
cppcoreguidelines-slicing,\
#cppcoreguidelines-avoid-magic-numbers,\
#cppcoreguidelines-init-variables,\
cppcoreguidelines-pro-type-cstyle-cast,\
cppcoreguidelines-pro-type-reinterpret-cast,\
cppcoreguidelines-explicit-virtual-functions,\
cppcoreguidelines-special-member-functions,\
cppcoreguidelines-pro-type-member-init,\
cert-err52-cpp,\
hicpp-deprecated-headers,\
cert-err34-c,\
cert-dcl50-cpp,\
cert-dcl58-cpp,\
bugprone-undefined-memory-manipulation,\
cert-exp42-c,\
cert-str34-c,\
cert-oop54-cpp,\
cert-con36-c,\
cert-msc50-cpp,\
cert-msc32-c,\
cert-env33-c,\
hicpp-no-malloc,\
hicpp-avoid-goto,\
hicpp-avoid-c-arrays,\
modernize-use-using,\
modernize-loop-convert,\
google-runtime-int,\
google-readability-casting,\
misc-unconventional-assign-operator,\
modernize-use-default-member-init,\
cppcoreguidelines-prefer-member-initializer,\
"
)

endfunction()


