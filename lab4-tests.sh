#!/bin/bash

mkdir tmp

rm -f comments.txt

# Colors for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Test result tracking
TOTAL_POINTS=0
MAX_POINTS=100

award_points() {
    local test_name=$1
    local points=$2
    local max_points=$3
    local details=$4
    
    TOTAL_POINTS=$((TOTAL_POINTS + points))
    if [ $points -eq $max_points ]; then
        echo -e "${GREEN}✓ $test_name test passed: $points/$max_points points${NC}"
        [ -n "$details" ] && echo -e "  Details: $details"

        echo -e "✓ $test_name test passed: $points/$max_points points" >> comments.txt
        [ -n "$details" ] && echo -e "  Details: $details" >> comments.txt
    else
        echo -e "${RED}✗ $test_name test failed: $points/$max_points points${NC}"
        [ -n "$details" ] && echo -e "  Error: $details"

        echo -e "✗ $test_name test failed: $points/$max_points points" >> comments.txt
        [ -n "$details" ] && echo -e "  Error: $details" >> comments.txt
    fi
}

echo -e "${BLUE}======================================${NC}"
echo -e "${BLUE}         ThreadPool Lab Tests         ${NC}"
echo -e "${BLUE}======================================${NC}\n"

timeout 60s bash -c 'echo -e "100\ntest.log\n1\n.txt\n1\n0\n2\n100\n9\n1\n4\n0\n" | ./client >tmp/test2 2>&1'
if [ $? -eq 124 ]; then
    award_points "Single threaded single account update" 0 5 "The command timed out after 60 seconds."
elif 
    grep -q "Interest update successful!" "tmp/test2" && \
    grep -q "Current balance: 101" "tmp/test2"; then
    award_points "Single threaded single account update" 5 5 "Successfully performed update"
else
    award_points "Single threaded single account update" 0 5 "Failed update"
fi

timeout 60s bash -c '
{
    echo "1000"
    echo "test.log"
    echo "1"
    echo ".txt"
    echo "1"
    echo "0"
    
    for i in {1..1000}; do
        echo "2"
        echo "100"
        echo "7"
        echo "1"
        echo "$i"
    done

    echo "7"
    echo "1"
    echo "0"
    echo "9"
    echo "2"
    echo "4"
    echo "0"
} | ./client > tmp/test3 2>&1'

# Check the result
if [ $? -eq 124 ]; then
    award_points "Multi-threaded multiple account update" 0 5 "The command timed out after 60 seconds."
elif grep -q "Interest update successful!" "tmp/test3" && \
    grep -q "Current balance: 101" "tmp/test3"; then
    award_points "Multi-threaded multiple account update" 5 5 "Successfully performed update"
else
    award_points "Multi-threaded multiple account update" 0 5 "Failed update"
fi


timeout 60s bash -c '
{
    echo "50"
    echo "test.log"
    echo "1"
    echo ".txt"
    echo "1"
    echo "0"
    
    for i in {1..50}; do
        echo "2"
        echo "100"
        echo "7"
        echo "1"
        echo "0"
        echo "9"
        echo "2"
        echo "7"
        echo "1"
        echo "$i"
    done

    echo "7"
    echo "1"
    echo "0"
    echo "4"
    echo "0"
} | ./client > tmp/test4 2>&1'

# Check the result
if [ $? -eq 124 ]; then
    award_points "Multi-threaded multiple account multi-update" 0 5 "The command timed out after 60 seconds."
elif grep -q "Interest update successful!" "tmp/test4" && \
    grep -q "Current balance: 164.463" "tmp/test4"; then
    award_points "Multi-threaded multiple account multi-update" 5 5 "Successfully performed update"
else
    award_points "Multi-threaded multiple account multi-update" 0 5 "Failed update"
fi



###
#formerly private tests
###

rm -f unit_test_results.txt
timeout 60s bash -c ./privatetest > unit_test_results.txt
if [ $? -eq 124 ]; then
    award_points "Private tests" 0 85 "The command timed out after 60 seconds."
else
    if grep -q "TEST: ThreadPool Constructor - PASSED ✓" "unit_test_results.txt"; then
        award_points "ThreadPool Constructor" 50 50 "Passed"
    else
        award_points "ThreadPool Constructor" 0 50 "Failed"
    fi
    if grep -q "TEST: ThreadPool Destructor - PASSED ✓" "unit_test_results.txt"; then
        award_points "ThreadPool Destructor" 20 20 "Passed"
    else
        award_points "ThreadPool Destructor" 0 20 "Failed"
    fi
    if grep -q "TEST: ThreadPool Enqueue - PASSED ✓" "unit_test_results.txt"; then
        award_points "ThreadPool Enqueue" 15 15 "Passed"
    else
        award_points "ThreadPool Enqueue" 0 15 "Failed"
    fi
fi











# Print summary
echo -e "\n${BLUE}=======================================${NC}"
echo -e "${BLUE}              Test Summary              ${NC}"
echo -e "${BLUE}=======================================${NC}"
echo -e "Points earned: ${TOTAL_POINTS}/${MAX_POINTS}\n"




###
#cleanup
###

rm -rf tmp
make -s clean >/dev/null 2>&1