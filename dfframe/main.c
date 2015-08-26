/**
 *****************************************************************************

 @file       main.c

 @brief      Main program

 @author 

 @date       2015-05-26

 @version    $Id: main.c,v 1.1 2015/08/26 00:47:43 guest Exp $

  ----------------------------------------------------------------------------
  RELEASE NOTE :

   DATE          REV    REMARK
  ============= ====== =======================================================
  26th May 2015  0.1    Experimental

 *****************************************************************************/

#include "dfframe.h"


/**
 * Main function
 */
int main (int argc, char **argv)
{

    // DirectFB の初期化
    init(&argc, &argv);

    // 画像の読み込み
    readImage(0, "pict.png");
    reanderImage(0, false);
    flip();

    // 裏面にも読み込み
    reanderImage(0, false);
    
    // ペンの色設定
    setColor(0, 0, 0, 0xff);

    while(1) {
        // タッチ入力待ち
        position_t p1, p2;
        p1 = eventLoop();

        while (getTouchState() == TOUCHED) {
            // 座標取得
            p2 = eventLoop();

            // 線描画
            line(p1, p2);

            // 画面切替
            flip();

            // 裏面にも線描画
            line(p1, p2);

            // 座標保存
            p1 = p2;
        }

    }

    // リソース解放
    release();

    return 0;

}

/* ------------------------------------------------------------------------- */
