# D3D-Map-Tool
D3D12 기반 실시간 맵 에디터 입니다.

## 소개 
게임 개발 및 시뮬레이션을 위한 맵 제작/편집 툴을 간단히 만들어보았습니다.
<img width="986" height="758" alt="스크린샷 2025-09-14 121751" src="https://github.com/user-attachments/assets/221a12c2-8509-4616-a9b6-1a23c916f31a" />

## 주요기능
1. 그레이 비트맵 이미지 파일로 지형 로드
2. 와이어프레임모드 변경
3. 마우스 우클릭 + 움직임 과 wasd로 카메라 움직임
4. 마우스 좌클릭으로 지형 높이 변경
5. 마우스 좌클릭으로 오브젝트 생성 및 삭제
6. 그레이 비트맵 이미지 파일로 지형 저장(일부 형식은 저장 X, 현재는 지형만 저장)



## 스크린샷
- 일반 텍스처를 적용한 모습
  <img width="547" height="412" alt="image" src="https://github.com/user-attachments/assets/712628f1-2ad4-4b5c-afd0-064af0169efc" />


- 로드한 그레이 비트맵 이미지를 통해 생성한 노말맵 텍스처를 적용한 모습
  
![image](https://github.com/user-attachments/assets/766a32df-0bb6-417e-90f3-d4e6f9cde6f8)

- 로드한 그레이 비트맵 이미지 텍스처를 적용한 모습
  
![image](https://github.com/user-attachments/assets/5a05cc55-dded-440c-b915-92e5c56faff5)

- 원하는 그레이 비트맵 파일을 읽어올 수 있습니다.
  
![openFile](https://github.com/user-attachments/assets/224b2565-a49d-4562-b762-8187505692ec)

- 맵에 변화를 주어도 노말맵에 바로 적용됩니다.
  
![ModHeight NormalMap](https://github.com/user-attachments/assets/24dfb1df-1676-4477-8293-6079b013bd57)

- 테셀레이션이 적용되었습니다.
  
![tess11](https://github.com/user-attachments/assets/d5e733a2-be70-4611-9edc-20edeb982bbf)

## 사용법
- [] 새 맵 생성 : 파일 실행 시 or Select Height Map Open
- [] 맵 편집 : 좌 상단의 스크롤 바를 움직여 조절
- [] 오브젝트 생성 및 삭제 : Change Mode 윈도우의 버튼을 통해 기능 변경

## 기술 설명
- 마우스 레이
  
  계산 셰이더를 이용해 지형의 버텍스버퍼와 인덱스버퍼를 순회하여 교점을 찾았습니다.
  지형의 Topology가 quad patch이므로 사각형이기 때문에 직선 삼각형 교차검사를 두번 진행했습니다.
  그렇게 획득한 교차점을 받아두고 마우스 클릭시 지형의 높이를 수정하는 계산 셰이더를 실행시켜 원하는 부분만 수정하도록 했습니다.
  
- QuadTree를 이용한 오브젝트 관리 최적화
  
  월드에 있는 모든 오브젝트를 한번에 관리할 경우 많은 수가 존재하면 연산 부하가 커지기 때문에 QuadTree를 이용해 월드를 4등분하여 관리했습니다.
  오브젝트를 생성할 때 마우스의 교차점이 4등분한 월드의 한 부분이 바운딩 박스와 교차하는 경우 생성 후 해당 리프가 갖고있는 배열에서 관리했습니다.

  삭제할 때도 마찬가지로 4등분한 월드와 교차하는 부분을 찾고 해당 리프의 배열에서 삭제했습니다.
  이때 배열에서 아무런 조치없이 제거하게되면 오버헤드가 발생하기 때문에 배열의 맨 뒤에 있는 요소를 제거할 요소의 위치로 옮긴 후
  pop하는 swap and pop 방식을 적용하여 오버헤드를 최소화했습니다.
  
- 테셀레이션 크랙 방지

  <img width="773" height="391" alt="image" src="https://github.com/user-attachments/assets/5c78a8bb-40c2-4ae9-988b-072b7a38c7b9" />

  > 위치값 A와 A`가 다르기 때문에 인접한 patch도 Tessellation factor의 값이 다를 수 있는데 같은 값을 갖는 변의 중심점을 적용하여 같은 값을 갖도록 했습니다.
  
  기존에는 quad patch의 중심점을 기준으로 tessellation factor를 계산하여 모든 변에 적용하였는데 이 경우 크랙이 발생하였습니다.
  그래서 quad patch의 변의 중심점을 기준으로 tessellation factor를 계산하여 인접한 변끼리는 동일한 값을 갖도록 하여 크랙을 방지하였습니다.
  
- 그림자 맵 및 SSAO 적용

  그림자 맵을 이용해서 움직이는 광원에 대한 그림자를 적용하였고
  SSAO를 통해 더 세세한 그림자를 추가했습니다.
  
- 높이맵을 이용한 노말맵 생성

  계산 셰이더에서 높이맵 텍스처의 텍셀 xy좌표값과 해당 좌표의 높이값을  x,y,z 벡터로 이용하여 노말값들을 저장하는 텍스처를 생성했습니다.
  실시간으로 높이맵이 변해도 노말값도 실시간으로 계산해서 적용됩니다.
  
## 참고 자료
- https://blog.naver.com/atxino/147347129
- https://github.com/microsoft/DirectXTex
