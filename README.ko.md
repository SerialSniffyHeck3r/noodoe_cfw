# FuckNudo CFW

[my-ak550]

**킴코 모터사이클에 장착된 순정 옵션 "Noodoe" 에 새 생명을 불어넣는 커스텀 펌웨어 프로젝트입니다.**


![속도 링과 트립 컴퓨터](Projects/documentations/manual/images/trip-a.png)


2017년 출시되어 약 6-7년의 기간동안 장착되어 출시되던 이 Noodoe 시스템은 2027년 KYMCO 사에서 서비스 종료를 결정했다. 이에 따라 기본적인 차량 설정 및 Noodoe의 대부분의 기능이 작동하지 않게 된다나 뭐라나. 근데 아직도 차량은 멀쩡한데 Noodoe를 사용할 수 없게 된다는 것은 좀 문제가 있다고 생각했다. 이걸 다시 사용해보자! 이참에 원래 Noodoe에서 없엇지만 추가적으로 있었으면 좋겠는 다양한 기능을 더 넣어보자! 라는 마인드로 시작한 프로젝트이다.

Noodoe는 생각보다 알찬 하드웨어 구성을 갖고 있다! STM32, 그래픽 컨트롤러, 디스플레이, 저장소를 가진 시스템으로써 계기판과 독립되어 휴대전화와 연동하는 기능을 가진 시스템이다. 

순정 펌웨어의 동작을 분석한 다음 하드웨어를 하나씩 브링업해 만든 대체 펌웨어와 안드로이드 컴패니언 앱으로 구성되어 있다.
아이폰용 앱은 없다. 이건 애플 잘못이지 내 잘못이 아니라는 점을 알아둬라ㅋ 



본 프로젝트를 설치하거나 사용하고 싶으시다면, 다음 문서를 참고하십시오:

[사용 설명서](Projects/documentations/manual/README.md)


## 분석 관련 문서

아래의 문서들은 본인의 연구 노트 중 일부를 적은 것이다. 전부 적은 것은 아니며 업데이트될 수 있다. 

| 주제 | 문서 |
|---|---|
| 실물 하드웨어 | [하드웨어 구조](Projects/documentations/hardware.ko.md) |
| 순정 동작 분석 | [역공학 과정](Projects/documentations/reverse-engineering.ko.md) |
| 어셈블리에서 하드웨어를 알아낸 방법 | [순정 펌웨어 어셈블리 분석](Projects/documentations/stock-assembly.ko.md) |
| CFW 구조 | [아키텍처](Projects/documentations/architecture.ko.md) |
| 메모리와 자산 | [메모리 지도](Projects/documentations/memory-map.ko.md) |
| 설치·롤백·복구 | [설치·복구 구조](Projects/documentations/installer.ko.md) |
| 버튼·차량 데이터 | [차량 인터페이스](Projects/documentations/vehicle-interface.ko.md) |

[English](README.md) · [최신 릴리스](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest)
