#!/bin/bash

# JPEG Encoder/Decoder Timing Test Script
# Runs encoding or decoding operations multiple times and calculates average timings

# Color codes for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Function to display usage
usage() {
    echo "Usage: $0 <operation> <file_path> <iterations> [sampling_factor]"
    echo ""
    echo "Parameters:"
    echo "  operation         : 'encode' or 'decode'"
    echo "  file_path        : Path to the input file (.bmp for encode, .jpg for decode)"
    echo "  iterations       : Number of times to run the operation (positive integer)"
    echo "  sampling_factor  : Optional, for encode only (1x1, 2x1, 1x2, 2x2), default: 1x1"
    echo ""
    echo "Examples:"
    echo "  $0 encode assets/cat_cpp.bmp 10 2x1"
    echo "  $0 decode assets/cat.jpg 20"
    exit 1
}

# Check if correct number of arguments
if [ $# -lt 3 ] || [ $# -gt 4 ]; then
    usage
fi

OPERATION=$1
FILE_PATH=$2
ITERATIONS=$3
SAMPLING_FACTOR=${4:-1x1}

# Validate operation type
if [ "$OPERATION" != "encode" ] && [ "$OPERATION" != "decode" ]; then
    echo -e "${RED}Error: Operation must be 'encode' or 'decode'${NC}"
    usage
fi

# Validate file exists
if [ ! -f "$FILE_PATH" ]; then
    echo -e "${RED}Error: File '$FILE_PATH' not found${NC}"
    exit 1
fi

# Validate iterations is a positive integer
if ! [[ "$ITERATIONS" =~ ^[0-9]+$ ]] || [ "$ITERATIONS" -lt 1 ]; then
    echo -e "${RED}Error: Iterations must be a positive integer${NC}"
    exit 1
fi

# Validate sampling factor
if [ "$OPERATION" == "encode" ]; then
    if [[ ! "$SAMPLING_FACTOR" =~ ^(1x1|2x1|1x2|2x2)$ ]]; then
        echo -e "${RED}Error: Sampling factor must be 1x1, 2x1, 1x2, or 2x2${NC}"
        exit 1
    fi
fi

# Check if binaries exist
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ENCODER_BIN="$SCRIPT_DIR/bin/encoder_c"
DECODER_BIN="$SCRIPT_DIR/bin/decoder_c"

if [ "$OPERATION" == "encode" ] && [ ! -f "$ENCODER_BIN" ]; then
    echo -e "${RED}Error: Encoder binary not found at $ENCODER_BIN${NC}"
    echo "Please run 'make encoder_c' first"
    exit 1
fi

if [ "$OPERATION" == "decode" ] && [ ! -f "$DECODER_BIN" ]; then
    echo -e "${RED}Error: Decoder binary not found at $DECODER_BIN${NC}"
    echo "Please run 'make decoder_c' first"
    exit 1
fi

# Initialize arrays for storing timing data
declare -a stage1_times
declare -a stage2_times
declare -a stage3_times
declare -a stage4_times
declare -a stage5_times
declare -a stage6_times
declare -a stage7_times
declare -a total_times
MCU_COUNT=0

# Create log file with timestamp
TIMESTAMP=$(date +"%Y-%m-%d_%H-%M-%S")
LOG_FILE="${TIMESTAMP}_timing.log"

# Display test information
echo -e "${BLUE}======================================${NC}"
echo -e "${BLUE}  JPEG Timing Test${NC}"
echo -e "${BLUE}======================================${NC}"
echo -e "Operation:       ${GREEN}$OPERATION${NC}"
echo -e "Input File:      ${GREEN}$FILE_PATH${NC}"
echo -e "Iterations:      ${GREEN}$ITERATIONS${NC}"
if [ "$OPERATION" == "encode" ]; then
    echo -e "Sampling Factor: ${GREEN}$SAMPLING_FACTOR${NC}"
fi
echo -e "Log File:        ${GREEN}$LOG_FILE${NC}"
echo -e "${BLUE}======================================${NC}"
echo ""

# Write log file header
{
    echo "======================================"
    echo "  JPEG Timing Test Log"
    echo "======================================"
    echo "Date/Time:       $(date '+%Y-%m-%d %H:%M:%S')"
    echo "Operation:       $OPERATION"
    echo "Input File:      $FILE_PATH"
    echo "Iterations:      $ITERATIONS"
    if [ "$OPERATION" == "encode" ]; then
        echo "Sampling Factor: $SAMPLING_FACTOR"
    fi
    echo "======================================"
    echo ""
} > "$LOG_FILE"

# Run the operations
echo -e "${YELLOW}Running $ITERATIONS iterations...${NC}"

# Write column headers to log file
if [ "$OPERATION" == "encode" ]; then
    echo "Iter    Stage1(ms)  Stage2(ms)  Stage3(ms)  Stage4(ms)  Stage5(ms)  Stage6(ms)  Stage7(ms)  Total(ms)" >> "$LOG_FILE"
    echo "        Read BMP    ColorConv   Downsample  ForwardDCT  Quantize    Huffman     WriteJPG" >> "$LOG_FILE"
else
    echo "Iter    Stage1(ms)  Stage2(ms)  Stage3(ms)  Stage4(ms)  Stage5(ms)  Stage6(ms)  Stage7(ms)  Total(ms)" >> "$LOG_FILE"
    echo "        ReadJPEG    HuffmanDec  Dequantize  InverseDCT  Upsample    ColorConv   WriteBMP" >> "$LOG_FILE"
fi
echo "----    ----------  ----------  ----------  ----------  ----------  ----------  ----------  ----------" >> "$LOG_FILE"

for i in $(seq 1 $ITERATIONS); do
    printf "\rProgress: [%3d/%3d] " $i $ITERATIONS
    
    # Run the appropriate operation and capture output
    if [ "$OPERATION" == "encode" ]; then
        OUTPUT=$("$ENCODER_BIN" "$FILE_PATH" "$SAMPLING_FACTOR" 2>&1)
    else
        OUTPUT=$("$DECODER_BIN" "$FILE_PATH" 2>&1)
    fi
    
    # Parse MCU count (only once)
    if [ $i -eq 1 ]; then
        MCU_COUNT=$(echo "$OUTPUT" | grep "Total MCUs processed:" | awk '{print $4}')
    fi
    
    # Parse timing values (remove 'ms' suffix)
    stage1=$(echo "$OUTPUT" | grep "Stage 1" | awk '{print $(NF-1)}')
    stage2=$(echo "$OUTPUT" | grep "Stage 2" | awk '{print $(NF-1)}')
    stage3=$(echo "$OUTPUT" | grep "Stage 3" | awk '{print $(NF-1)}')
    stage4=$(echo "$OUTPUT" | grep "Stage 4" | awk '{print $(NF-1)}')
    stage5=$(echo "$OUTPUT" | grep "Stage 5" | awk '{print $(NF-1)}')
    stage6=$(echo "$OUTPUT" | grep "Stage 6" | awk '{print $(NF-1)}')
    stage7=$(echo "$OUTPUT" | grep "Stage 7" | awk '{print $(NF-1)}')
    total=$(echo "$OUTPUT" | grep "Total Time:" | awk '{print $(NF-1)}')
    
    # Store timing values
    stage1_times+=($stage1)
    stage2_times+=($stage2)
    stage3_times+=($stage3)
    stage4_times+=($stage4)
    stage5_times+=($stage5)
    stage6_times+=($stage6)
    stage7_times+=($stage7)
    total_times+=($total)
    
    # Write to log file
    printf "%4d    %10.4f  %10.4f  %10.4f  %10.4f  %10.4f  %10.4f  %10.4f  %10.4f\n" \
        $i "$stage1" "$stage2" "$stage3" "$stage4" "$stage5" "$stage6" "$stage7" "$total" >> "$LOG_FILE"
done

echo ""
echo -e "${GREEN}✓ Completed all iterations${NC}"
echo ""

# Calculate averages using awk (works on minimal systems without bc)
calc_avg() {
    printf '%s\n' "$@" | awk '{sum += $1; count++} END {if (count > 0) printf "%.4f", sum/count; else print "0"}'
}

avg_stage1=$(calc_avg "${stage1_times[@]}")
avg_stage2=$(calc_avg "${stage2_times[@]}")
avg_stage3=$(calc_avg "${stage3_times[@]}")
avg_stage4=$(calc_avg "${stage4_times[@]}")
avg_stage5=$(calc_avg "${stage5_times[@]}")
avg_stage6=$(calc_avg "${stage6_times[@]}")
avg_stage7=$(calc_avg "${stage7_times[@]}")
avg_total=$(calc_avg "${total_times[@]}")

# Write averages to log file
{
    echo ""
    echo "======================================"
    echo "  Average Results"
    echo "======================================"
    echo "Total MCUs processed: $MCU_COUNT"
    echo ""
    if [ "$OPERATION" == "encode" ]; then
        printf "Stage 1 (Read BMP):            %10.4f ms\n" "$avg_stage1"
        printf "Stage 2 (Color Conversion):    %10.4f ms\n" "$avg_stage2"
        printf "Stage 3 (Downsampling):        %10.4f ms\n" "$avg_stage3"
        printf "Stage 4 (Forward DCT):         %10.4f ms\n" "$avg_stage4"
        printf "Stage 5 (Quantization):        %10.4f ms\n" "$avg_stage5"
        printf "Stage 6 (Huffman Encoding):    %10.4f ms\n" "$avg_stage6"
        printf "Stage 7 (Write JPG):           %10.4f ms\n" "$avg_stage7"
    else
        printf "Stage 1 (Read JPEG):           %10.4f ms\n" "$avg_stage1"
        printf "Stage 2 (Huffman Decoding):    %10.4f ms\n" "$avg_stage2"
        printf "Stage 3 (Dequantization):      %10.4f ms\n" "$avg_stage3"
        printf "Stage 4 (Inverse DCT):         %10.4f ms\n" "$avg_stage4"
        printf "Stage 5 (Upsampling):          %10.4f ms\n" "$avg_stage5"
        printf "Stage 6 (Color Conversion):    %10.4f ms\n" "$avg_stage6"
        printf "Stage 7 (Write BMP):           %10.4f ms\n" "$avg_stage7"
    fi
    echo "--------------------------------------"
    printf "Total Time:                    %10.4f ms\n" "$avg_total"
    echo "======================================"
} >> "$LOG_FILE"

# Display results
echo -e "${BLUE}======================================${NC}"
echo -e "${BLUE}  Average Timing Results${NC}"
echo -e "${BLUE}======================================${NC}"
echo ""
echo -e "Total MCUs processed: ${GREEN}$MCU_COUNT${NC}"
echo ""
if [ "$OPERATION" == "encode" ]; then
    echo -e "${YELLOW}=== Encoder Average Timing (${ITERATIONS} iterations) ===${NC}"
    printf "Stage 1 (Read BMP):            %8s ms\n" "$avg_stage1"
    printf "Stage 2 (Color Conversion):    %8s ms\n" "$avg_stage2"
    printf "Stage 3 (Downsampling):        %8s ms\n" "$avg_stage3"
    printf "Stage 4 (Forward DCT):         %8s ms\n" "$avg_stage4"
    printf "Stage 5 (Quantization):        %8s ms\n" "$avg_stage5"
    printf "Stage 6 (Huffman Encoding):    %8s ms\n" "$avg_stage6"
    printf "Stage 7 (Write JPG):           %8s ms\n" "$avg_stage7"
else
    echo -e "${YELLOW}=== Decoder Average Timing (${ITERATIONS} iterations) ===${NC}"
    printf "Stage 1 (Read JPEG):           %8s ms\n" "$avg_stage1"
    printf "Stage 2 (Huffman Decoding):    %8s ms\n" "$avg_stage2"
    printf "Stage 3 (Dequantization):      %8s ms\n" "$avg_stage3"
    printf "Stage 4 (Inverse DCT):         %8s ms\n" "$avg_stage4"
    printf "Stage 5 (Upsampling):          %8s ms\n" "$avg_stage5"
    printf "Stage 6 (Color Conversion):    %8s ms\n" "$avg_stage6"
    printf "Stage 7 (Write BMP):           %8s ms\n" "$avg_stage7"
fi
echo "----------------------------------"
printf "Total Time:                    %8s ms\n" "$avg_total"
echo -e "${BLUE}======================================${NC}"
echo ""
echo -e "${GREEN}✓ Log file saved: $LOG_FILE${NC}"
echo ""
