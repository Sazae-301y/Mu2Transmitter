/**
 * @file mu_wrapper.hpp
 * @author K.Fukushima@nnct
 * @brief
 * MUの送信データの生成、受信データの解析を行う。送受信はライブラリ外部で行う。
 * @version 0.1
 * @date 2024-06-01
 * @copyright Copyright (c) 2024 改変自由
 *
 */
// 
#ifndef MU_H_
#define MU_H_
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/**
 * @brief エラーコード、コールバックでエラー発生時、dataのバイト0で通知
 *
 */
enum MUError : uint8_t {
  /// エラーなし
  MU_ERR_NONE = 0,
  /// 16進数以外の文字が含まれている
  MU_ERR_LENGTH_NOT_HEX,
  /// 識別できないコマンド
  MU_ERR_INVALID_COMMAND,
  /// IRコマンドを受信（送信エラー）
  MU_ERR_CATCH_IR,
  /// 受信データ長が最大値を超えている
  MU_ERR_LENGTH_TOO_LONG,
  /// フッタがCRLFでない
  MU_ERR_TAIL_NOT_CRLF,
};
/**
 * @brief MUのコールバック条件
 *
 */
enum MUEvent : uint8_t { // コールバックで通知されるイベント
  /// イベントなし
  MU_EVENT_NONE = 0,
  /// 送信要求(data,lenに送信バイナリが格納されている)
  MU_EVENT_SEND_REQUEST,
  /// 受信完了,dataからデータ読み出し可能
  MU_EVENT_RX_COMPLETE,
  /// エラー発生,dataにエラーコード
  MU_EVENT_ERROR,
};

/**
 * @brief MUのコールバック関数,イベントとデータを通知
 *
 */
typedef void (*MUCallback)(MUEvent event, uint8_t *data, uint8_t len);

// コンフィグ
constexpr uint8_t MU_MAX_DATALEN = 12;
// 送信データサイズ、
constexpr uint8_t MU_MAX_COMMANDBUF = MU_MAX_DATALEN + 5;

/**
 * @brief MUのデータ分析、生成を管理するクラス
 *
 */
class MUWrapper {
public:
  /**
   * @brief クラス生成時に呼び出し、コールバック関数を登録
   *
   * @param callback
   */
  MUWrapper(MUCallback callback) : callback(callback){};
  /**
   * @brief
   * MUの初期化。チャンネル設定用データの生成とコールバック呼び出しを行う。
   *
   * @param ch MUのチャンネル。周波数との対応はMUのデータシートを参照。
   */
  void init(uint8_t ch) {
    setParam("GI", 4);
    setParam("CH", ch);
    setParam("DI", 1);
    setParam("EI", 0);
  };
  void setParams(uint8_t gi,uint8_t ch, uint8_t di,uint8_t ei){
    setParam("GI", gi);
    setParam("CH", ch);
    setParam("DI", di);
    setParam("EI", ei);
  };
  /**
   * @brief
   * MUからのデータを解析し、コールバックで通知する。エラー発生時はエラー通知を行う。
   *
   * @param data UARTからのデータ配列の先頭アドレスを渡す
   * @param len データ長
   * @return true 未使用
   * @return false 未使用
   */
  bool pushRawData(uint8_t *data, uint8_t len) {
    // staticのついた変数は初回実行時のみ初期化され、次回以降は前回実行時の値が保持される
    static phase_t phase = PHASE_WAIT_HEAD;
    static char command_char[4] = {0};
    static char length_char[3] = {0};
    static uint8_t buf[MU_MAX_DATALEN];
    static uint8_t length_reported = 0;
    static uint8_t length = 0;
    static char footer_char[3] = {0};
    while (len--) {          // 長さが0になるまで繰り返す
      uint8_t d = *(data++); // ポインタを進めながらデータを読み出す
      switch (phase) {       // 読み出しの段階で分岐
      case PHASE_WAIT_HEAD: // 受信データの頭を検出
        if (d == '*') {     // 受信開始は'*'で検出
          phase = PHASE_COMMAND;
          memset(length_char, 0, 3);
          memset(command_char, 0, 4);
          memset(footer_char, 0, 3);
        }
        break;
      case PHASE_COMMAND: // 受信データの種類（エラー・データ受信）の区別
        if (!((d >= 'A' && d <= 'Z') ||
              d == '=')) { // コマンドは大文字アルファベットと'='のみ
          error(MU_ERR_INVALID_COMMAND);
          phase = PHASE_WAIT_HEAD;
          break;
        }
        strcat(command_char, (char *)&d);
        if (strncmp(command_char, "DR=", 3)==0) { // データ受信コマンド
          phase = PHASE_LENGTH;
          break;
        }
        if (strncmp(command_char, "IR", 2)==0) { // IRコマンドは送信エラーを示す
          phase = PHASE_TAIL;
          error(MU_ERR_CATCH_IR);
          break;
        }
        if (strlen(command_char) == 3) {
          error(MU_ERR_INVALID_COMMAND); // 区別されていないコマンド
          phase = PHASE_WAIT_HEAD; // あきらめて次のデータを待つ
        }
        break;
      case PHASE_LENGTH: // データ長の読み出し
        if (!((d >= '0' && d <= '9') ||
              (d >= 'A' && d <= 'F'))) { // データ長は16進数で表される
          error(MU_ERR_LENGTH_NOT_HEX); // 不適切なデータ
          phase = PHASE_WAIT_HEAD;
          break;
        }
        strcat(length_char, (char *)&d); // 文字列に追加
        if (strlen(length_char) == 2) {  // データ長は必ず2文字
          phase = PHASE_DATA;
          length_reported = (uint8_t)strtol(length_char, NULL,
                                            16); // 16進数文字列を数値に変換
          if (length_reported >
              MU_MAX_DATALEN) { // データ長が設定の最大値を超えている
            error(MU_ERR_LENGTH_TOO_LONG);
            phase = PHASE_WAIT_HEAD;
            break;
          }
          length = 0;
        }
        break;
      case PHASE_DATA:     // データ部分の読み出し
        buf[length++] = d; // 代入後にインクリメント
        if (length_reported == length) {
          phase =
              PHASE_TAIL_HASDATA; // データ後に改行コードのフッタがあるためフッタ検出まで待機
        }
        break;
      case PHASE_TAIL:         // 改行コード検出
      case PHASE_TAIL_HASDATA: // データ後の改行コード検出
        strcat(footer_char, (char *)&d);
        if (strlen(footer_char) == 2) {
          if (strncmp(footer_char, "\r\n", 2)==0) {
            // 受信完了後の処理を書く
            if (phase == PHASE_TAIL_HASDATA) { // データありの場合はデータを通知
              callback(MU_EVENT_RX_COMPLETE, buf, length_reported);
            }
            phase = PHASE_WAIT_HEAD;
            break;
          }
          error(MU_ERR_TAIL_NOT_CRLF); // 改行コードがCRLFでない！？
        }
        break;
      }
    }
    return true;
  }
  /**
   * @brief
   * MUへのデータ送信。送信用データの生成と送信コールバック呼び出しを行う。
   *
   * @param data
   * @param len
   * @return true
   * @return false
   */
  bool send(uint8_t *data, uint8_t len) { // MUにデータ送信
    uint8_t buf[6] = {0};
    sprintf((char *)buf, "@DT%02X", len);
    callback(MU_EVENT_SEND_REQUEST, buf, 5);
    callback(MU_EVENT_SEND_REQUEST, data, len);
    sprintf((char *)buf, "\r\n");
    callback(MU_EVENT_SEND_REQUEST, buf, 2);
    return true;
  }

private:
  /**
   * @brief データの解析の段階
   *
   */
  enum phase_t : uint8_t {
    PHASE_WAIT_HEAD,
    PHASE_COMMAND,
    PHASE_LENGTH,
    PHASE_DATA,
    PHASE_TAIL,
    PHASE_TAIL_HASDATA,
  };
  /**
   * @brief コールバック関数のポインタ保管
   *
   */
  MUCallback callback = nullptr;
  /**
   * @brief 設定などのコマンド生成
   *
   * @param command
   * @param value
   * @param valueLength
   */
  void sendCommand(const char command[3], char *value, uint8_t valueLength) {
    uint8_t buffer[16];
    buffer[0] = (uint8_t)'@';
    memcpy(buffer + 1, command, 2);
    memcpy(buffer + 3, value, valueLength);
    buffer[3 + valueLength] = '\r';
    buffer[4 + valueLength] = '\n';
    callback(MU_EVENT_SEND_REQUEST, buffer, 5 + valueLength);
  }
  /**
   * @brief チャンネルなどパラメータの設定
   *
   * @param param
   * @param value
   */
  void setParam(const char param[3], uint8_t value) {
    char value_char[3] = {0};
    sprintf(value_char, "%02X", value);
    sendCommand(param, value_char, 2);
  }
  /**
   * @brief エラー通知
   *
   * @param e
   */
  void error(MUError e) {
    uint8_t err = (uint8_t)e;
    callback(MU_EVENT_ERROR, &err, 1);
  }
};
#endif /* MU_H_ */
