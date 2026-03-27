#include <Arduino.h>

const static unsigned SysWinchMaxQt = 24;

typedef enum _WinchTypeEnum {
  WT_STEEL_ROPE,
  WT_CHAIN
} EWinchType;

typedef enum _WinchStateEnum {
  WS_UNCONNECTED,
  WS_WORK,
  WS_UP_LIMIT,
  WS_DOWN_LIMIT,
  WS_FAULT
} EWinchState;

typedef struct _WinchStruct {
  EWinchType Type = WT_STEEL_ROPE;
  EWinchState State = WS_UNCONNECTED;
} Winch_t;

typedef struct _WinchControllerStruct {
  Winch_t Winch[SysWinchMaxQt] = {};
  const static uint8_t WinchMaxQt = 3;
} WinchController_t;