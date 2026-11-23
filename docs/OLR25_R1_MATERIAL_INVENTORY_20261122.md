# OLR R1 — статический перечень материалов, 22.11.2026

Сгенерировано tools/inventory_olr_materials.py из локальных метаданных. Native статусы условны: настройки blend/detail и runtime coverage ещё требуют проверки. 96 используемых alias; определения могут встречаться в неиспользуемых ресурсах. Script-only/generated материалы не включены без runtime-доказательств.

| Alias | Класс | Статус | Источников ссылок |
|---|---|---|---:|
| def_shaders\def_add | S_SET | unclassified | 6 |
| def_shaders\def_aref | LM_AREF | native_opaque_aref_only | 18 |
| def_shaders\def_aref_v | V_AREF | native_opaque_aref_only | 13 |
| def_shaders\def_objects_lod | D_TREE | native_tree | 4 |
| def_shaders\def_refl_alpha_heli | LmEbB | forward_environment_unverified | 3 |
| def_shaders\def_refl_trans | LmEbB | forward_environment_unverified | 1 |
| def_shaders\def_trans | LM_AREF | native_opaque_aref_only | 17 |
| def_shaders\def_trans_v | V_AREF | native_opaque_aref_only | 9 |
| def_shaders\def_trans_v_tuchi | V_AREF | native_opaque_aref_only | 1 |
| def_shaders\def_vertex | V | native_vertex_conditional | 19 |
| defaref | LM_AREF | native_opaque_aref_only | 3 |
| default | LM | native_lmap_conditional | 18 |
| details\lod | S_SET | native_lod | 14 |
| effects\flare | S_SET | lua_forward_or_deferred_unverified | 1 |
| effects\glow | S_SET | unclassified | 17 |
| effects\lightplanes | V | lua_forward_or_deferred_unverified | 2 |
| effects\ryska |  | lua_forward_or_deferred_unverified | 3 |
| effects\swamp |  | unclassified | 1 |
| effects\wallmarkblend | S_SET | lua_forward_or_deferred_unverified | 3 |
| effects\watdc |  | unclassified | 2 |
| effects\water | S_SET | lua_forward_or_deferred_unverified | 6 |
| effects\watertest-1 | V_AREF | native_opaque_aref_only | 1 |
| flora\leaf_wave | D_TREE | native_tree | 14 |
| flora\trunk_wave | D_TREE | native_tree | 14 |
| levels\agroprom_asfalt | BmmDold | native_implicit_requires_detail_metadata | 1 |
| levels\agroprom_grass | BmmDold | native_implicit_requires_detail_metadata | 2 |
| levels\bar_grass | BmmDold | native_implicit_requires_detail_metadata | 1 |
| levels\darkvalley_grass | BmmDold | native_implicit_requires_detail_metadata | 1 |
| levels\darkvalley_sand | BmmDold | native_implicit_requires_detail_metadata | 1 |
| levels\escape_asfalt | BmmDold | native_implicit_requires_detail_metadata | 3 |
| levels\escape_grass | BmmDold | native_implicit_requires_detail_metadata | 4 |
| levels\frodo_generators | BmmDold | native_implicit_requires_detail_metadata | 1 |
| levels\frodo_generators_road | BmmDold | native_implicit_requires_detail_metadata | 1 |
| levels\garbage_earth | BmmDold | native_implicit_requires_detail_metadata | 1 |
| levels\l01_escape_grass | BmmDold | native_implicit_requires_detail_metadata | 1 |
| levels\military_asfalt | BmmDold | native_implicit_requires_detail_metadata | 1 |
| levels\mp_garbage_asfalt | BmmDold | native_implicit_requires_detail_metadata | 1 |
| levels\mp_rostok_asfalt | BmmDold | native_implicit_requires_detail_metadata | 1 |
| levels\mp_rostok_grass | BmmDold | native_implicit_requires_detail_metadata | 1 |
| levels\mp_rostok_sand | BmmDold | native_implicit_requires_detail_metadata | 1 |
| levels\pripyat_asfalt | BmmDold | native_implicit_requires_detail_metadata | 1 |
| levels\radar_grass | BmmDold | native_implicit_requires_detail_metadata | 1 |
| levels\stancia | BmmDold | native_implicit_requires_detail_metadata | 1 |
| levels\stancia_asfalt | BmmDold | native_implicit_requires_detail_metadata | 1 |
| levels\stancia_kanal | BmmDold | native_implicit_requires_detail_metadata | 1 |
| levels\yantar_asfalt | BmmDold | native_implicit_requires_detail_metadata | 1 |
| levels\yantar_earth | BmmDold | native_implicit_requires_detail_metadata | 1 |
| model\selflightl |  | lua_forward_or_deferred_unverified | 1 |
| models\antigas_glass | MODELEbB | missing_model_environment | 1 |
| models\artef | MODELEbB | missing_model_environment | 1 |
| models\artefact | MODELEbB | missing_model_environment | 1 |
| models\artefact2 | MODELEbB | missing_model_environment | 1 |
| models\bloodinv |  | lua_forward_or_deferred_unverified | 1 |
| models\eyeflight |  | lua_forward_or_deferred_unverified | 1 |
| models\kamflight |  | lua_forward_or_deferred_unverified | 1 |
| models\light_red |  | lua_forward_or_deferred_unverified | 1 |
| models\lightcone | MODEL | missing_model_ros_projector_shadows | 1 |
| models\lightplanes | MODEL | lua_forward_or_deferred_unverified | 1 |
| models\maref | MODEL | missing_model_ros_projector_shadows | 1 |
| models\miror |  | unclassified | 1 |
| models\mirror | MODELEbB | missing_model_environment | 1 |
| models\model | MODEL | missing_model_ros_projector_shadows | 1 |
| models\model_aref | MODEL | missing_model_ros_projector_shadows | 1 |
| models\model_fur | MODEL | missing_model_ros_projector_shadows | 1 |
| models\model_puh | MODEL | missing_model_ros_projector_shadows | 1 |
| models\model_refl | MODEL | missing_model_ros_projector_shadows | 1 |
| models\pautina | MODEL | lua_forward_or_deferred_unverified | 1 |
| models\selflight | MODEL | lua_forward_or_deferred_unverified | 1 |
| models\selflightl | MODEL | lua_forward_or_deferred_unverified | 1 |
| models\shotg_hud |  | unclassified | 1 |
| models\svetofour |  | lua_forward_or_deferred_unverified | 1 |
| models\transparent | MODEL | missing_model_ros_projector_shadows | 1 |
| models\water | MODELEbB | missing_model_environment | 1 |
| models\weapo | MODELEbB | missing_model_environment | 1 |
| models\weapons | MODELEbB | missing_model_environment | 1 |
| models\wind33 | MODELEbB | missing_model_environment | 1 |
| models\window | MODELEbB | missing_model_environment | 1 |
| models\xanomaly | MODEL | lua_forward_or_deferred_unverified | 1 |
| models\xdistort | MODEL | lua_forward_or_deferred_unverified | 1 |
| models\xdistortcolor | MODEL | lua_forward_or_deferred_unverified | 1 |
| models\xdistortcolorl | MODEL | lua_forward_or_deferred_unverified | 1 |
| models\xdistortcolorlinv | MODEL | lua_forward_or_deferred_unverified | 1 |
| models\xdistortinv | MODEL | lua_forward_or_deferred_unverified | 1 |
| models\xglass | MODEL | missing_model_ros_projector_shadows | 1 |
| models\xglass2 | MODEL | missing_model_ros_projector_shadows | 1 |
| models\xmonolith | MODEL | lua_forward_or_deferred_unverified | 1 |
| models\xwindows | MODEL | lua_forward_or_deferred_unverified | 1 |
| particles\add | PARTICLE | native_particle_cpp_independent_only_scene_unverified | 1 |
| particles\alpha_add | PARTICLE | native_particle_cpp_independent_only_scene_unverified | 1 |
| particles\blend | PARTICLE | native_particle_cpp_independent_only_scene_unverified | 1 |
| particles\set | PARTICLE | native_particle_cpp_independent_only_scene_unverified | 1 |
| particles\xadd | S_SET | lua_forward_or_deferred_unverified | 1 |
| particles\xblend | S_SET | unclassified | 1 |
| particles\xdistort | S_SET | lua_forward_or_deferred_unverified | 1 |
| selflight | V | lua_forward_or_deferred_unverified | 9 |
| selflight_for_sar_lava |  | unclassified | 1 |
