# Gregorian calendar and RTC adapter

`BSP_Calendar.c/h`는 하드웨어·RTOS·heap에 의존하지 않는 Gregorian 달력이다.
년1..9999, 월/일/요일/시/분/초를 갖고, 요일은 월요일1..일요일7이다.
윤년은 4년 규칙과 100년 예외, 400년 예외의 예외를 모두 반영한다.

- IsLeapYear / DaysInMonth / Weekday: 유효 범위 및 실제 달력 계산.
- Normalize: 날짜와 시간을 검사하고 요일을 계산한다. 실패하면 입력 보존.
- AddSeconds / AddDays: 음수와 양수 이동, 월/년 경계 처리, 범위 초과 거부.
  성공 시에만 출력을 바꾸며 입력·출력 포인터가 같아도 된다.

초 계산은64bit이며2038 문제를 만들지 않는다. INT64_MIN/MAX를 더하기 전에
허용 범위를 검사한다. 연도 검색과 월 이동은 유한하게 제한된다.
시간대·DST·윤초는 이 달력의 기능이 아니다.

`BSP_Clock`은 실제 STM32 RTC의 두 자리 연도를 **2000..2099**로 해석한다.
일반 달력이9999년까지 지원한다고 하드웨어의 세기 보존까지 구현했다고
주장하지 않는다. 2100년 이후 영속 시간은 별도 세기 저장 정책이 필요하다.
Set 입력 요일0(생략) 또는1..7은 실제 년월일에서 다시 계산한다. Read도 요일을
계산하며, 기존 RTC에 잘못 저장된 요일을 읽었다는 이유로 RTC를 쓰지 않는다.
`NoodoeSystemSnapshot.weekday`는 기존 구조 끝에 추가해 년월일·요일·시간을
중간/상위 계층에 함께 제공한다.

검증: calendar_host는 실제 ARM C를 O0/Os로 실행하고 Python datetime과 대조한다.
각각51011개 assertion으로1..9999년 전체의 주요 날짜, 2100/2400 윤년, 경계와
1009개 signed-second 벡터를 검사한다. clock_host의86개 assertion은 RTC
read 순서·오류 및 요일 정규화를 검증한다. 이 시험은 실제 RTC 날짜를 쓰지 않는다.
