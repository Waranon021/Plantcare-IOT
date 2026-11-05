import fs from "node:fs";
import assert from "node:assert/strict";
const source=new URL("../node-red/flows.json",import.meta.url);
const flow=JSON.parse(fs.readFileSync(source,"utf8").replace(/^\uFEFF/,""));
const code=flow.find(n=>n.name==="PlantCare - Check HTTPS Ingest").func;
const run=new Function("msg","env",code);
const testKey="TEST_ONLY_NOT_A_DEPLOYMENT_SECRET_000";
const meta={plant_id:"plant01",device_id:"esp32_01",data_source:"real"};
const soil={sensor_ok:true,raw:2468,filtered_raw:2466,relative_wetness_pct:43.53};
const thermal={pixels:Array.from({length:64},(_,i)=>i%8<4?26:29)};
const invoke=(payload,provided=testKey)=>run({payload,req:{headers:{"x-plantcare-key":provided}}},{get:()=>testKey});
const only=invoke({...meta,soil});
assert.equal(only[0].statusCode,202);
assert.deepEqual(only[0].payload,{ok:true,accepted:true,soil:true,thermal:false});
assert.equal(only[2],null);
const combined=invoke({...meta,soil,thermal});
assert.equal(combined[0].statusCode,202);
const {t_left_c,t_right_c,t_min_c,t_max_c,delta_t_c,...pixelFields}=combined[2].payload[0];
assert.deepEqual({t_left_c,t_right_c,t_min_c,t_max_c,delta_t_c},
  {t_left_c:26,t_right_c:29,t_min_c:26,t_max_c:29,delta_t_c:3});
const orderedPixels=Array.from({length:64},(_,i)=>20+i/10);
const matrix=invoke({...meta,thermal:{pixels:orderedPixels}});
assert.equal(matrix[0].statusCode,202);
assert.equal(matrix[1],null);
const fields=matrix[2].payload[0];
assert.equal(Object.keys(fields).filter(k=>/^p[0-7][0-7]$/.test(k)).length,64);
for(const [index,value] of orderedPixels.entries()){
  assert.equal(fields["p"+Math.floor(index/8)+index%8],value);
}
for(const [input,key,error,status] of [
 [{...meta,soil},"wrong","invalid_api_key",401],
 ["{",testKey,"invalid_json",400],
 [{...meta,device_id:"wrong",soil},testKey,"invalid_metadata",400],
 [{...meta,soil:{...soil,raw:4096}},testKey,"invalid_soil",400],
 [{...meta,thermal:{pixels:[1]}},testKey,"invalid_thermal",400],
 [meta,testKey,"no_sensor_data",400]
]){
 const r=invoke(input,key);
 assert.equal(r[0].statusCode,status);
 assert.equal(r[0].payload.error,error);
 assert.equal(r[1],null);
 assert.equal(r[2],null);
}
console.log("9 offline ingest cases passed, including all 64 pixel positions");
