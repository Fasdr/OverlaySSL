# Project paths
BUILD_DIR = build
# Arch-specific package names for the check
ARCH_PKGS = qt6-base xautomation libx11 cmake gcc

all: check_arch_deps build_all

check_arch_deps:
	@if [ -f /etc/arch-release ]; then \
		echo "Arch Linux detected. Verifying packages..."; \
		for pkg in $(ARCH_PKGS); do \
			pacman -Qs $$pkg > /dev/null || { \
				echo "Error: Package '$$pkg' is missing."; \
				echo "Please run: sudo pacman -S qt6-base xautomation libx11 cmake gcc"; \
				exit 1; \
			}; \
		done; \
		echo "All Arch dependencies are satisfied."; \
	else \
		echo "Non-Arch system detected. Skipping specific dependency check..."; \
	fi

build_all:
	@echo "Step 1: Preparing build directory..."
	@mkdir -p $(BUILD_DIR)
	@echo "Step 2: Running CMake configuration..."
	@cd $(BUILD_DIR) && cmake .. > /dev/null
	@echo "Step 3: Compiling Overlay and Spy Library..."
	@cd $(BUILD_DIR) && make --no-print-directory

clean:
	@echo "Cleaning project..."
	@rm -rf $(BUILD_DIR)
	@echo "Clean complete."

.PHONY: all check_arch_deps build_all clean