format:
	find src -iname '*.h' -o -iname '*.c' | xargs clang-format -i

lint:
	cppcheck --enable=all --suppress=missingIncludeSystem src
