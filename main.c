#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// -------------------- 全域與結構 --------------------

// 棋盤大小 = n(n+1)/2
static int boardSize;

// 動態配置的棋盤：board[r][c] = 0 表示尚未放置
// 如果放置了第 k 個方塊，則值為 k (方塊的ID)
static int **board = NULL;

// squaresCount[i]：邊長 (i+1) 的方塊剩餘數量
static int *squaresCount = NULL;

// nValue: 使用者輸入的 n
static int nValue;

// 需要放的方塊總數 = 1 + 2 + ... + n
static int totalSquares;

// 目前已放多少塊
static int placedSoFar = 0;

// 記錄每個方塊的資訊
typedef struct {
    int id;   // 第幾個放(1-based)
    int size; // 方塊邊長
    int row;  // 左上角 row (0-based)
    int col;  // 左上角 col (0-based)
} PiecePlacement;

// placements[k]: 第 (k+1) 個方塊的資訊
static PiecePlacement *placements = NULL;

// 已找到的解數量（每找到一組解就 +1）
static long long solutionCount = 0;

// 記錄程式開始時間
static clock_t start_time;

// -------------------- 工具函式 --------------------

// 計算 1+2+...+x
int sumOf1toN(int x) {
    return x * (x + 1) / 2;
}

// 檢查能否在 (r, c) 放 size x size
int canPlace(int r, int c, int size) {
    if (r + size > boardSize || c + size > boardSize) {
        return 0; // 超出邊界
    }
    for (int rr = 0; rr < size; rr++) {
        for (int cc = 0; cc < size; cc++) {
            if (board[r + rr][c + cc] != 0) {
                return 0; // 已被其他方塊佔用
            }
        }
    }
    return 1;
}

// 將 size x size 填入(或清除)特定 ID
// val = (piece ID) 表示放置, val=0 表示移除
void fillSquare(int r, int c, int size, int val) {
    for (int rr = 0; rr < size; rr++) {
        for (int cc = 0; cc < size; cc++) {
            board[r + rr][c + cc] = val;
        }
    }
}

// 找下一個空位 (board[r][c] == 0)
int findNextEmpty(int *outR, int *outC) {
    for (int rr = 0; rr < boardSize; rr++) {
        for (int cc = 0; cc < boardSize; cc++) {
            if (board[rr][cc] == 0) {
                *outR = rr;
                *outC = cc;
                return 1;
            }
        }
    }
    return 0; // 已無空位
}

// -------------------- JSON 輸出 --------------------
// 每組解，輸出成 solution_X.json
void outputSolutionAsJSON(long long solIndex) {
    char filename[64];
    sprintf(filename, "solution_%lld.json", solIndex);

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "無法開啟檔案 %s\n", filename);
        return;
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"solutionIndex\": %lld,\n", solIndex);
    fprintf(fp, "  \"boardSize\": %d,\n", boardSize);
    fprintf(fp, "  \"placements\": [\n");

    for (int i = 0; i < totalSquares; i++) {
        int id   = placements[i].id;
        int sz   = placements[i].size;
        int row  = placements[i].row;
        int col  = placements[i].col;

        fprintf(fp, "    {\"id\":%d, \"size\":%d, \"row\":%d, \"col\":%d}",
                id, sz, row, col);

        if (i < totalSquares - 1) {
            fprintf(fp, ",\n");
        } else {
            fprintf(fp, "\n");
        }
    }

    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");

    fclose(fp);
}

// -------------------- SVG 輸出 --------------------
// 每組解對應 solution_X.svg
// - 背景白色
// - 先畫淡灰色格線 (#cccccc, stroke-width="0.5")
// - 再畫方塊 (半透明 fill="hsla(...)") 
// - 在方塊中央只顯示「方塊大小」
void outputSolutionAsSVG(long long solIndex) {
    const int cellSize = 20; 
    const int margin = 10;

    int svgWidth  = margin * 2 + boardSize * cellSize;
    int svgHeight = margin * 2 + boardSize * cellSize;

    char filename[64];
    sprintf(filename, "solution_%lld.svg", solIndex);

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "無法開啟檔案 %s\n", filename);
        return;
    }

    // SVG header
    fprintf(fp, "<?xml version=\"1.0\" standalone=\"no\"?>\n");
    fprintf(fp,
        "<svg width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\" "
        "xmlns=\"http://www.w3.org/2000/svg\">\n",
        svgWidth, svgHeight, svgWidth, svgHeight
    );

    // 背景
    fprintf(fp,
        "  <rect x=\"0\" y=\"0\" width=\"%d\" height=\"%d\" fill=\"white\"/>\n",
        svgWidth, svgHeight
    );

    // (1) 淡灰格線: 先畫在背景上
    fprintf(fp, "  <!-- grid lines -->\n");
    for (int i = 0; i <= boardSize; i++) {
        int x = margin + i * cellSize;
        int y = margin + i * cellSize;

        // 垂直線
        fprintf(fp,
            "  <line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" "
            "stroke=\"#cccccc\" stroke-width=\"0.5\"/>\n",
            x, margin, x, margin + boardSize * cellSize
        );
        // 水平線
        fprintf(fp,
            "  <line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" "
            "stroke=\"#cccccc\" stroke-width=\"0.5\"/>\n",
            margin, y, margin + boardSize * cellSize, y
        );
    }

    // (2) 繪製每個方塊(半透明)
    fprintf(fp, "  <!-- squares -->\n");
    for (int i = 0; i < totalSquares; i++) {
        int sz   = placements[i].size; // 邊長
        int row  = placements[i].row;  // 0-based
        int col  = placements[i].col;  // 0-based

        int x = margin + col * cellSize;
        int y = margin + row * cellSize;
        int w = sz * cellSize;
        int h = sz * cellSize;

        int hue = (sz * 40) % 360;
		int saturation = 90;
		int lightness = 60;

        // 用 hsla: alpha=0.6，讓底下格線可見
        fprintf(fp,
            "  <rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" "
            "fill=\"hsla(%d,%d%%,%d%%,0.6)\" stroke=\"black\" stroke-width=\"1\"/>\n",
            x, y, w, h, hue, saturation, lightness);
    }

    // (3) 在方塊中央顯示 "size"
    fprintf(fp, "  <!-- text labels (show size) -->\n");
    for (int i = 0; i < totalSquares; i++) {
        int sz  = placements[i].size;
        int row = placements[i].row;
        int col = placements[i].col;

        int x = margin + col * cellSize;
        int y = margin + row * cellSize;
        int w = sz * cellSize;
        int h = sz * cellSize;

        fprintf(fp,
            "  <text x=\"%d\" y=\"%d\" font-size=\"%d\" "
            "text-anchor=\"middle\" dominant-baseline=\"middle\" fill=\"black\">%d</text>\n",
            x + w / 2,      // 中心 x
            y + h / 2,      // 中心 y
            (cellSize / 2), // 字體大小
            sz              // 顯示方塊大小
        );
    }

    // 結尾
    fprintf(fp, "</svg>\n");
    fclose(fp);
}

// -------------------- 回溯列舉所有解 --------------------
void dfsAllSolutions() {
    // 若已放完所有方塊 => 找到一組解
    if (placedSoFar == totalSquares) {
        solutionCount++;

        // 計算目前執行時間
        double elapsed_time = (double)(clock() - start_time) / CLOCKS_PER_SEC;

        // 終端簡要資訊
        printf("第 %lld 組解，已執行 %.2f 秒，solution_%lld.svg, solution_%lld.json\n",
               solutionCount, elapsed_time, solutionCount, solutionCount);

        // 輸出 SVG
        outputSolutionAsSVG(solutionCount);
        // 輸出 JSON
        outputSolutionAsJSON(solutionCount);

        // 回溯繼續找下一組
        return;
    }

    // 找下一個空位
    int r, c;
    if (!findNextEmpty(&r, &c)) {
        // 沒空位卻沒放完 => 死路
        return;
    }

    // 大到小嘗試
    for (int i = nValue - 1; i >= 0; i--) {
        if (squaresCount[i] > 0) {
            int sz = i + 1; // 邊長
            if (canPlace(r, c, sz)) {
                squaresCount[i]--;
                placedSoFar++;

                int thisID = placedSoFar;
                fillSquare(r, c, sz, thisID);

                placements[placedSoFar - 1].id   = thisID;
                placements[placedSoFar - 1].size = sz;
                placements[placedSoFar - 1].row  = r;
                placements[placedSoFar - 1].col  = c;

                dfsAllSolutions();

                // 回溯
                fillSquare(r, c, sz, 0);
                squaresCount[i]++;
                placedSoFar--;
            }
        }
    }
}

// -------------------- main --------------------
int main() {
    printf("請輸入 n：");
    if (scanf("%d", &nValue) != 1 || nValue < 1) {
        printf("輸入有誤\n");
        return 1;
    }

    boardSize    = sumOf1toN(nValue); // n(n+1)/2
    totalSquares = boardSize;         // 1+2+...+n = boardSize

    // 記錄程式開始時間
    start_time = clock();

    // 配置棋盤
    board = (int**)malloc(sizeof(int*) * boardSize);
    for (int i = 0; i < boardSize; i++) {
        board[i] = (int*)calloc(boardSize, sizeof(int));
    }

    // squaresCount
    squaresCount = (int*)malloc(sizeof(int) * nValue);
    for (int i = 0; i < nValue; i++) {
        squaresCount[i] = i + 1; 
    }

    // placements
    placements = (PiecePlacement*)malloc(sizeof(PiecePlacement) * totalSquares);

    printf("開始尋找所有解...\n\n");
    dfsAllSolutions();

    if (solutionCount == 0) {
        printf("沒有找到任何解 (或尚未搜完就結束)\n");
    } else {
        printf("=== 總共找到 %lld 組解 ===\n", solutionCount);
    }

    // 釋放
    for (int i = 0; i < boardSize; i++) {
        free(board[i]);
    }
    free(board);
    free(squaresCount);
    free(placements);

    return 0;
}
