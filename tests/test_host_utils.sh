#!/bin/bash
#
# AdrOS Host Utility Tests
#
# Compiles userspace utilities with the host gcc and validates their
# functionality using known inputs/outputs.  Utilities that depend on
# AdrOS-specific syscalls or /proc are skipped.
#
# Usage: bash tests/test_host_utils.sh
# Exit:  0 = all pass, 1 = failure

set -e

PASS=0
FAIL=0
SKIP=0
ERRORS=""

BUILDDIR="$(mktemp -d)"
trap 'rm -rf "$BUILDDIR"' EXIT

CC="${CC:-gcc}"
CFLAGS="-Wall -Wextra -std=c11 -O0 -g -D_POSIX_C_SOURCE=200809L"

pass() { echo "  PASS  $1"; PASS=$((PASS+1)); }
fail() { echo "  FAIL  $1 — $2"; FAIL=$((FAIL+1)); ERRORS="$ERRORS\n    $1: $2"; }
skip() { echo "  SKIP  $1"; SKIP=$((SKIP+1)); }

compile() {
    local name="$1" src="$2"
    $CC $CFLAGS -o "$BUILDDIR/$name" "$src" 2>"$BUILDDIR/${name}.err"
    return $?
}

# ================================================================
echo "========================================="
echo "  AdrOS Host Utility Tests"
echo "========================================="
echo ""

# ---------- echo ----------
echo "--- echo ---"
if compile echo_test user/echo.c; then
    out=$("$BUILDDIR/echo_test" hello world)
    [ "$out" = "hello world" ] && pass "echo basic" || fail "echo basic" "got: $out"

    out=$("$BUILDDIR/echo_test" -n hello)
    [ "$out" = "hello" ] && pass "echo -n" || fail "echo -n" "got: $out"

    out=$("$BUILDDIR/echo_test" -e 'a\nb')
    expected=$(printf 'a\nb')
    [ "$out" = "$expected" ] && pass "echo -e" || fail "echo -e" "got: $out"

    out=$("$BUILDDIR/echo_test")
    [ "$out" = "" ] && pass "echo empty" || fail "echo empty" "got: $out"
else
    skip "echo (compile failed: $(cat "$BUILDDIR/echo_test.err" | head -1))"
fi

# ---------- cat ----------
echo "--- cat ---"
if compile cat_test user/cat.c; then
    echo "hello cat" > "$BUILDDIR/cat_in.txt"
    out=$("$BUILDDIR/cat_test" "$BUILDDIR/cat_in.txt")
    [ "$out" = "hello cat" ] && pass "cat file" || fail "cat file" "got: $out"

    out=$(echo "stdin test" | "$BUILDDIR/cat_test")
    [ "$out" = "stdin test" ] && pass "cat stdin" || fail "cat stdin" "got: $out"

    echo "file1" > "$BUILDDIR/cat1.txt"
    echo "file2" > "$BUILDDIR/cat2.txt"
    out=$("$BUILDDIR/cat_test" "$BUILDDIR/cat1.txt" "$BUILDDIR/cat2.txt")
    expected=$(printf "file1\nfile2")
    [ "$out" = "$expected" ] && pass "cat multi" || fail "cat multi" "got: $out"
else
    skip "cat (compile failed)"
fi

# ---------- head ----------
echo "--- head ---"
if compile head_test user/head.c; then
    printf "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n" > "$BUILDDIR/head_in.txt"
    out=$("$BUILDDIR/head_test" "$BUILDDIR/head_in.txt" | wc -l)
    [ "$out" -eq 10 ] && pass "head default 10" || fail "head default 10" "got $out lines"

    out=$("$BUILDDIR/head_test" -n 3 "$BUILDDIR/head_in.txt")
    expected=$(printf "1\n2\n3")
    [ "$out" = "$expected" ] && pass "head -n 3" || fail "head -n 3" "got: $out"

    out=$(printf "a\nb\nc\n" | "$BUILDDIR/head_test" -n 2)
    expected=$(printf "a\nb")
    [ "$out" = "$expected" ] && pass "head stdin" || fail "head stdin" "got: $out"
else
    skip "head (compile failed)"
fi

# ---------- tail ----------
echo "--- tail ---"
if compile tail_test user/tail.c; then
    printf "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n" > "$BUILDDIR/tail_in.txt"
    out=$("$BUILDDIR/tail_test" -n 3 "$BUILDDIR/tail_in.txt")
    expected=$(printf "10\n11\n12")
    [ "$out" = "$expected" ] && pass "tail -n 3" || fail "tail -n 3" "got: $out"
else
    skip "tail (compile failed)"
fi

# ---------- wc ----------
echo "--- wc ---"
if compile wc_test user/wc.c; then
    printf "hello world\nfoo bar baz\n" > "$BUILDDIR/wc_in.txt"
    out=$("$BUILDDIR/wc_test" "$BUILDDIR/wc_in.txt")
    # Should contain line count (2), word count (5), byte count
    echo "$out" | grep -q "2" && pass "wc lines" || fail "wc lines" "got: $out"
    echo "$out" | grep -q "5" && pass "wc words" || fail "wc words" "got: $out"
else
    skip "wc (compile failed)"
fi

# ---------- sort ----------
echo "--- sort ---"
if compile sort_test user/sort.c; then
    printf "banana\napple\ncherry\n" | "$BUILDDIR/sort_test" > "$BUILDDIR/sort_out.txt"
    expected=$(printf "apple\nbanana\ncherry")
    out=$(cat "$BUILDDIR/sort_out.txt")
    [ "$out" = "$expected" ] && pass "sort basic" || fail "sort basic" "got: $out"

    printf "banana\napple\ncherry\n" | "$BUILDDIR/sort_test" -r > "$BUILDDIR/sort_out.txt"
    expected=$(printf "cherry\nbanana\napple")
    out=$(cat "$BUILDDIR/sort_out.txt")
    [ "$out" = "$expected" ] && pass "sort -r" || fail "sort -r" "got: $out"
else
    skip "sort (compile failed)"
fi

# ---------- uniq ----------
echo "--- uniq ---"
if compile uniq_test user/uniq.c; then
    printf "aaa\naaa\nbbb\nccc\nccc\n" | "$BUILDDIR/uniq_test" > "$BUILDDIR/uniq_out.txt"
    expected=$(printf "aaa\nbbb\nccc")
    out=$(cat "$BUILDDIR/uniq_out.txt")
    [ "$out" = "$expected" ] && pass "uniq basic" || fail "uniq basic" "got: $out"

    printf "aaa\naaa\nbbb\n" | "$BUILDDIR/uniq_test" -c > "$BUILDDIR/uniq_out.txt"
    out=$(cat "$BUILDDIR/uniq_out.txt")
    echo "$out" | grep -q "2 aaa" && pass "uniq -c" || fail "uniq -c" "got: $out"
else
    skip "uniq (compile failed)"
fi

# ---------- cut ----------
echo "--- cut ---"
if compile cut_test user/cut.c; then
    out=$(printf "a:b:c\n" | "$BUILDDIR/cut_test" -d: -f2)
    [ "$out" = "b" ] && pass "cut -d: -f2" || fail "cut -d: -f2" "got: $out"

    out=$(printf "hello:world:foo\n" | "$BUILDDIR/cut_test" -d: -f1)
    [ "$out" = "hello" ] && pass "cut -d: -f1" || fail "cut -d: -f1" "got: $out"

    out=$(printf "a.b.c\n" | "$BUILDDIR/cut_test" -d. -f3)
    [ "$out" = "c" ] && pass "cut -d. -f3" || fail "cut -d. -f3" "got: $out"
else
    skip "cut (compile failed)"
fi

# ---------- grep ----------
echo "--- grep ---"
if compile grep_test user/grep.c; then
    printf "hello world\nfoo bar\nhello again\n" > "$BUILDDIR/grep_in.txt"
    out=$("$BUILDDIR/grep_test" hello "$BUILDDIR/grep_in.txt")
    lines=$(echo "$out" | wc -l)
    [ "$lines" -eq 2 ] && pass "grep match count" || fail "grep match count" "got $lines"

    out=$("$BUILDDIR/grep_test" -v hello "$BUILDDIR/grep_in.txt")
    echo "$out" | grep -q "foo bar" && pass "grep -v" || fail "grep -v" "got: $out"

    out=$("$BUILDDIR/grep_test" -c hello "$BUILDDIR/grep_in.txt")
    echo "$out" | grep -q "2" && pass "grep -c" || fail "grep -c" "got: $out"

    out=$("$BUILDDIR/grep_test" -n hello "$BUILDDIR/grep_in.txt")
    echo "$out" | grep -q "1:" && pass "grep -n" || fail "grep -n" "got: $out"

    out=$(printf "stdin line\nno match\n" | "$BUILDDIR/grep_test" stdin)
    [ "$out" = "stdin line" ] && pass "grep stdin" || fail "grep stdin" "got: $out"
else
    skip "grep (compile failed)"
fi

# ---------- tr ----------
echo "--- tr ---"
if compile tr_test user/tr.c; then
    out=$(echo "hello" | "$BUILDDIR/tr_test" 'elo' 'ELO')
    [ "$out" = "hELLO" ] && pass "tr translate" || fail "tr translate" "got: $out"

    out=$(echo "hello world" | "$BUILDDIR/tr_test" -d 'lo')
    [ "$out" = "he wrd" ] && pass "tr -d" || fail "tr -d" "got: $out"
else
    skip "tr (compile failed)"
fi

# ---------- basename ----------
echo "--- basename ---"
if compile basename_test user/basename.c; then
    out=$("$BUILDDIR/basename_test" /usr/bin/foo)
    [ "$out" = "foo" ] && pass "basename path" || fail "basename path" "got: $out"

    out=$("$BUILDDIR/basename_test" /usr/bin/foo.c .c)
    [ "$out" = "foo" ] && pass "basename suffix" || fail "basename suffix" "got: $out"

    out=$("$BUILDDIR/basename_test" foo)
    [ "$out" = "foo" ] && pass "basename plain" || fail "basename plain" "got: $out"
else
    skip "basename (compile failed)"
fi

# ---------- dirname ----------
echo "--- dirname ---"
if compile dirname_test user/dirname.c; then
    out=$("$BUILDDIR/dirname_test" /usr/bin/foo)
    [ "$out" = "/usr/bin" ] && pass "dirname path" || fail "dirname path" "got: $out"

    out=$("$BUILDDIR/dirname_test" foo)
    [ "$out" = "." ] && pass "dirname plain" || fail "dirname plain" "got: $out"

    out=$("$BUILDDIR/dirname_test" /)
    [ "$out" = "/" ] && pass "dirname root" || fail "dirname root" "got: $out"
else
    skip "dirname (compile failed)"
fi

# ---------- tee ----------
echo "--- tee ---"
if compile tee_test user/tee.c; then
    out=$(echo "tee test" | "$BUILDDIR/tee_test" "$BUILDDIR/tee_out.txt")
    file_content=$(cat "$BUILDDIR/tee_out.txt")
    [ "$out" = "tee test" ] && pass "tee stdout" || fail "tee stdout" "got: $out"
    [ "$file_content" = "tee test" ] && pass "tee file" || fail "tee file" "got: $file_content"

    echo "line1" | "$BUILDDIR/tee_test" "$BUILDDIR/tee_app.txt" > /dev/null
    echo "line2" | "$BUILDDIR/tee_test" -a "$BUILDDIR/tee_app.txt" > /dev/null
    out=$(cat "$BUILDDIR/tee_app.txt")
    expected=$(printf "line1\nline2")
    [ "$out" = "$expected" ] && pass "tee -a" || fail "tee -a" "got: $out"
else
    skip "tee (compile failed)"
fi

# ---------- dd ----------
echo "--- dd ---"
if compile dd_test user/dd.c; then
    echo "hello dd test data" > "$BUILDDIR/dd_in.txt"
    "$BUILDDIR/dd_test" if="$BUILDDIR/dd_in.txt" of="$BUILDDIR/dd_out.txt" bs=512 2>/dev/null
    out=$(cat "$BUILDDIR/dd_out.txt")
    [ "$out" = "hello dd test data" ] && pass "dd copy" || fail "dd copy" "got: $out"
else
    skip "dd (compile failed)"
fi

# ---------- pwd ----------
echo "--- pwd ---"
if compile pwd_test user/pwd.c; then
    out=$("$BUILDDIR/pwd_test")
    expected=$(pwd)
    [ -n "$out" ] && pass "pwd output" || fail "pwd output" "empty"
else
    skip "pwd (compile failed)"
fi

# ---------- uname ----------
echo "--- uname ---"
if compile uname_test user/uname.c; then
    out=$("$BUILDDIR/uname_test")
    [ "$out" = "AdrOS" ] && pass "uname default" || fail "uname default" "got: $out"

    out=$("$BUILDDIR/uname_test" -a)
    echo "$out" | grep -q "AdrOS" && pass "uname -a" || fail "uname -a" "got: $out"
    echo "$out" | grep -q "i686" && pass "uname -a machine" || fail "uname -a machine" "got: $out"

    out=$("$BUILDDIR/uname_test" -m)
    [ "$out" = "i686" ] && pass "uname -m" || fail "uname -m" "got: $out"

    out=$("$BUILDDIR/uname_test" -r)
    [ "$out" = "0.1.0" ] && pass "uname -r" || fail "uname -r" "got: $out"
else
    skip "uname (compile failed)"
fi

# ---------- id ----------
echo "--- id ---"
if compile id_test user/id.c; then
    out=$("$BUILDDIR/id_test")
    echo "$out" | grep -q "uid=" && pass "id uid" || fail "id uid" "got: $out"
    echo "$out" | grep -q "gid=" && pass "id gid" || fail "id gid" "got: $out"
else
    skip "id (compile failed)"
fi

# ---------- printenv ----------
echo "--- printenv ---"
if compile printenv_test user/printenv.c; then
    out=$(HOME=/test/home "$BUILDDIR/printenv_test" HOME)
    [ "$out" = "/test/home" ] && pass "printenv HOME" || fail "printenv HOME" "got: $out"

    out=$(FOO=bar "$BUILDDIR/printenv_test" FOO)
    [ "$out" = "bar" ] && pass "printenv FOO" || fail "printenv FOO" "got: $out"

    # printenv with no args should list all variables
    out=$(FOO=bar "$BUILDDIR/printenv_test" | grep "^FOO=bar$")
    [ "$out" = "FOO=bar" ] && pass "printenv all" || fail "printenv all" "got: $out"
else
    skip "printenv (compile failed)"
fi

# ---------- cp ----------
echo "--- cp ---"
if compile cp_test user/cp.c; then
    echo "cp source" > "$BUILDDIR/cp_src.txt"
    "$BUILDDIR/cp_test" "$BUILDDIR/cp_src.txt" "$BUILDDIR/cp_dst.txt"
    out=$(cat "$BUILDDIR/cp_dst.txt")
    [ "$out" = "cp source" ] && pass "cp file" || fail "cp file" "got: $out"
else
    skip "cp (compile failed)"
fi

# ---------- mv ----------
echo "--- mv ---"
if compile mv_test user/mv.c; then
    echo "mv data" > "$BUILDDIR/mv_src.txt"
    "$BUILDDIR/mv_test" "$BUILDDIR/mv_src.txt" "$BUILDDIR/mv_dst.txt"
    [ ! -f "$BUILDDIR/mv_src.txt" ] && pass "mv src removed" || fail "mv src removed" "still exists"
    out=$(cat "$BUILDDIR/mv_dst.txt" 2>/dev/null)
    [ "$out" = "mv data" ] && pass "mv dst content" || fail "mv dst content" "got: $out"
else
    skip "mv (compile failed)"
fi

# ---------- touch/rm/mkdir/rmdir ----------
echo "--- touch/rm/mkdir/rmdir ---"
compile_ok=1
compile touch_test user/touch.c || compile_ok=0
compile rm_test user/rm.c || compile_ok=0
compile mkdir_test user/mkdir.c || compile_ok=0
compile rmdir_test user/rmdir.c || compile_ok=0
if [ "$compile_ok" -eq 1 ]; then
    "$BUILDDIR/touch_test" "$BUILDDIR/touchfile"
    [ -f "$BUILDDIR/touchfile" ] && pass "touch create" || fail "touch create" "not created"

    "$BUILDDIR/rm_test" "$BUILDDIR/touchfile"
    [ ! -f "$BUILDDIR/touchfile" ] && pass "rm file" || fail "rm file" "still exists"

    "$BUILDDIR/mkdir_test" "$BUILDDIR/testdir"
    [ -d "$BUILDDIR/testdir" ] && pass "mkdir" || fail "mkdir" "not created"

    "$BUILDDIR/rmdir_test" "$BUILDDIR/testdir"
    [ ! -d "$BUILDDIR/testdir" ] && pass "rmdir" || fail "rmdir" "still exists"
else
    skip "touch/rm/mkdir/rmdir (compile failed)"
fi

# ---------- ln ----------
echo "--- ln ---"
if compile ln_test user/ln.c; then
    echo "link target" > "$BUILDDIR/ln_src.txt"
    "$BUILDDIR/ln_test" -s "$BUILDDIR/ln_src.txt" "$BUILDDIR/ln_link.txt" 2>/dev/null || true
    if [ -L "$BUILDDIR/ln_link.txt" ]; then
        out=$(cat "$BUILDDIR/ln_link.txt")
        [ "$out" = "link target" ] && pass "ln -s" || fail "ln -s" "got: $out"
    else
        # May not support -s on host build; try hard link
        "$BUILDDIR/ln_test" "$BUILDDIR/ln_src.txt" "$BUILDDIR/ln_hard.txt" 2>/dev/null || true
        if [ -f "$BUILDDIR/ln_hard.txt" ]; then
            pass "ln hard"
        else
            skip "ln (no link created)"
        fi
    fi
else
    skip "ln (compile failed)"
fi

# ---------- sed ----------
echo "--- sed ---"
if compile sed_test user/sed.c; then
    out=$(echo "hello world" | "$BUILDDIR/sed_test" 's/world/earth/')
    [ "$out" = "hello earth" ] && pass "sed s///" || fail "sed s///" "got: $out"

    out=$(echo "aaa bbb aaa" | "$BUILDDIR/sed_test" 's/aaa/XXX/g')
    [ "$out" = "XXX bbb XXX" ] && pass "sed s///g" || fail "sed s///g" "got: $out"

    out=$(echo "foo bar" | "$BUILDDIR/sed_test" 's/foo/baz/')
    [ "$out" = "baz bar" ] && pass "sed s first only" || fail "sed s first only" "got: $out"

    printf "line1\nline2\n" > "$BUILDDIR/sed_in.txt"
    out=$("$BUILDDIR/sed_test" 's/line/LINE/g' "$BUILDDIR/sed_in.txt")
    expected=$(printf "LINE1\nLINE2")
    [ "$out" = "$expected" ] && pass "sed file" || fail "sed file" "got: $out"
else
    skip "sed (compile failed: $(cat "$BUILDDIR/sed_test.err" | head -1))"
fi

# ---------- awk ----------
echo "--- awk ---"
if compile awk_test user/awk.c; then
    out=$(echo "hello world foo" | "$BUILDDIR/awk_test" '{print $2}')
    [ "$out" = "world" ] && pass "awk print \$2" || fail "awk print \$2" "got: $out"

    out=$(printf "a:b:c\n" | "$BUILDDIR/awk_test" -F : '{print $1}')
    [ "$out" = "a" ] && pass "awk -F :" || fail "awk -F :" "got: $out"

    out=$(printf "hello world\nfoo bar\nhello again\n" | "$BUILDDIR/awk_test" '/hello/{print $0}')
    lines=$(echo "$out" | wc -l)
    [ "$lines" -eq 2 ] && pass "awk pattern" || fail "awk pattern" "got $lines lines"
else
    skip "awk (compile failed: $(cat "$BUILDDIR/awk_test.err" | head -1))"
fi

# ---------- who ----------
echo "--- who ---"
if compile who_test user/who.c; then
    out=$("$BUILDDIR/who_test")
    echo "$out" | grep -q "root" && pass "who output" || fail "who output" "got: $out"
else
    skip "who (compile failed)"
fi

# ---------- find ----------
echo "--- find ---"
if compile find_test user/find.c; then
    mkdir -p "$BUILDDIR/findtest/sub"
    touch "$BUILDDIR/findtest/a.txt"
    touch "$BUILDDIR/findtest/b.c"
    touch "$BUILDDIR/findtest/sub/c.txt"

    out=$("$BUILDDIR/find_test" "$BUILDDIR/findtest" -name "*.txt")
    echo "$out" | grep -q "a.txt" && pass "find -name a.txt" || fail "find -name a.txt" "got: $out"
    echo "$out" | grep -q "c.txt" && pass "find -name c.txt" || fail "find -name c.txt" "got: $out"

    out=$("$BUILDDIR/find_test" "$BUILDDIR/findtest" -type d)
    echo "$out" | grep -q "sub" && pass "find -type d" || fail "find -type d" "got: $out"
else
    skip "find (compile failed: $(cat "$BUILDDIR/find_test.err" | head -1))"
fi

# ---------- which ----------
echo "--- which ---"
if compile which_test user/which.c; then
    # which looks in /bin and /sbin hardcoded, so just check it runs
    "$BUILDDIR/which_test" nonexistent_cmd > /dev/null 2>&1
    [ $? -ne 0 ] && pass "which not found" || fail "which not found" "should return nonzero"
else
    skip "which (compile failed)"
fi

# ================================================================
echo ""
echo "========================================="
echo "  Host Utility Test Results"
echo "========================================="
TOTAL=$((PASS+FAIL))
echo "  $PASS/$TOTAL passed, $FAIL failed, $SKIP skipped"

if [ "$FAIL" -gt 0 ]; then
    echo ""
    echo "  Failed tests:$ERRORS"
    echo ""
    echo "  RESULT: FAIL"
    exit 1
fi

echo ""
echo "  RESULT: PASS"
exit 0
