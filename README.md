# PCGDemo
PCG学习及作品集
## GrammarLearningMap
展示了使用PCG图复刻城市案例中的Kit_Bldg_CHA，但由于资产大小限制，库中没有包含相关资产，请使用CitySample案例Migrate"Kit_Bldg_CHA.umap"及其关联资产到项目中，将GrammarLearning内容作为子关卡挂载。
在本地图中CityExampleLearning文件夹下包含所有使用的资产，本案例完全使用UE自带图表节点完成，具体包括：
1. BP_BuildingWithDir：基础蓝图，指定方向数据传输给PCGGraph
2. Struct_ModulesWithMesh：手动创建的结构体包含使用Grammar语法时ModulesInfo的基础内容并附加了Mesh软引用，是各楼层配置DataTable(/Game/CityExampleLearning/MultiFloor/Floors)的源结构
3. SingleFloor：使用语法、DataTable生成**单层建筑**的简单示例，是MultiFloor的原型
4. MultiFloor：使用语法、DataTable生成**多层建筑**的示例，在SingleFloor的基础上进行优化和其他输入输出修改，成为PCG_CHA_BaseFloor基础子图表。PCH_MultiFloor层名称进行筛选多次调用基础子图标形成多层建筑。
[[UE5]CitySample复刻计划(1)-PCGGrammar尝试](https://www.bilibili.com/opus/1106025637196333056)
## 程序化道路生成
基于用户输入样条生成道路、交汇路口、街区，使用UEC++实现。
[[UE5]CitySample复刻计划(2)-纯UECpp下的道路生成](https://www.bilibili.com/opus/1116054145300693027)
[[UE5]CitySample复刻计划(3)-纯UECpp下的街区生成](https://www.bilibili.com/read/cv43301770) 
[[UE5]CitySample复刻计划(4)-街区生成细化中的UDynamicMesh和USplineComponent使用](https://www.bilibili.com/opus/1127339025877696515)
