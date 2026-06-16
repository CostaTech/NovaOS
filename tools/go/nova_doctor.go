package main

import (
	"fmt"
	"os"
	"path/filepath"
)

func exists(path string) bool {
	_, err := os.Stat(path)
	return err == nil
}

func check(path string) bool {
	if exists(path) {
		fmt.Printf("[OK]   %s\n", path)
		return true
	}
	fmt.Printf("[MISS] %s\n", path)
	return false
}

func main() {
	root := "."
	if len(os.Args) > 1 {
		root = os.Args[1]
	}

	items := []string{
		"Makefile",
		"build_NovaOS.sh",
		"run_NovaOS.sh",
		"boot/entry.asm",
		"include/nova.h",
		"kernel/main.cpp",
		"drivers/keyboard.c",
		"drivers/mouse.c",
		"fs/ramfs.cpp",
		"graphics/desktop.cpp",
		"shell/shell.cpp",
		"tools/lnp_tool.py",
	}

	ok := 0
	for _, item := range items {
		if check(filepath.Join(root, item)) {
			ok++
		}
	}

	fmt.Printf("Nova Doctor: %d/%d checks passed\n", ok, len(items))
	if ok != len(items) {
		os.Exit(1)
	}
}
