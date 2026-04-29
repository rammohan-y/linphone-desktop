#!/bin/bash
# Initializes all submodules one at a time with retries.
# Run from the linphone-desktop root directory.

MAX_RETRIES=10
RETRY_DELAY=3
ROOT_DIR="$(pwd)"

export GIT_HTTP_CONNECT_TIMEOUT=15
export GIT_HTTP_LOW_SPEED_LIMIT=1000
export GIT_HTTP_LOW_SPEED_TIME=10
git config --global http.connectTimeout 15
git config --global http.lowSpeedLimit 1000
git config --global http.lowSpeedTime 10

init_one() {
  local repo_dir="$1"
  local sub_path="$2"
  local full_path="$repo_dir/$sub_path"
  local attempt=1

  # Check if already populated (has more than just .git)
  local file_count
  file_count=$(find "$full_path" -maxdepth 1 -not -name '.git' -not -path "$full_path" 2>/dev/null | wc -l)
  if [ "$file_count" -gt 0 ]; then
    echo "=== SKIP (already populated): $full_path ==="
    return 0
  fi

  while true; do
    echo "=== [$attempt/$MAX_RETRIES] $full_path ==="
    if (cd "$repo_dir" && git submodule update --init --force "$sub_path") 2>&1; then
      # Verify it actually has content now
      file_count=$(find "$full_path" -maxdepth 1 -not -name '.git' -not -path "$full_path" 2>/dev/null | wc -l)
      if [ "$file_count" -gt 0 ]; then
        echo "=== OK: $full_path ==="
        return 0
      else
        echo "    Init returned success but directory still empty"
      fi
    fi

    if [ "$attempt" -ge "$MAX_RETRIES" ]; then
      echo "=== FAILED after $MAX_RETRIES attempts: $full_path ==="
      return 1
    fi

    attempt=$((attempt + 1))
    echo "    Retrying in ${RETRY_DELAY}s..."
    sleep "$RETRY_DELAY"
  done
}

process_repo() {
  local repo_dir="$1"

  if [ ! -f "$repo_dir/.gitmodules" ]; then
    return
  fi

  echo ""
  echo "=============================="
  echo " Submodules in $repo_dir"
  echo "=============================="

  git config -f "$repo_dir/.gitmodules" --get-regexp 'submodule\..*\.path' | awk '{print $2}' | while read -r sub_path; do
    init_one "$repo_dir" "$sub_path"
    # Recurse into the submodule for deeper nesting
    process_repo "$repo_dir/$sub_path"
  done
}

# Start from root
process_repo "$ROOT_DIR"

echo ""
echo "=============================="
echo " All done!"
echo "=============================="

# Final check — report any still-empty submodules
echo ""
echo "Checking for empty submodules..."
git submodule foreach --recursive --quiet '
  count=$(find . -maxdepth 1 -not -name ".git" -not -path "." | wc -l)
  if [ "$count" -eq 0 ]; then
    echo "EMPTY: $toplevel/$sm_path"
  fi
' 2>/dev/null

echo "Done."
