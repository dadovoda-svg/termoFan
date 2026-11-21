#pragma once
#include <Arduino.h>

const char portalStyle[] PROGMEM = R"CSS(
*{box-sizing:border-box}body{margin:0;background:#f1f5f5;color:#173133;font:16px/1.5 "Inter",system-ui,sans-serif}
header{background:#153e40;color:white;border-bottom:4px solid #51c5aa}
.wrap,main{max-width:60rem;margin:auto;padding:1.4rem clamp(1rem,4vw,2.5rem)}
header .wrap{display:flex;align-items:center;gap:1rem}header strong{font-size:1.45rem;letter-spacing:-.05em}header span{color:#c4dedc;border-left:1px solid #709594;padding-left:1rem}
h1{margin:0 0 .9rem;font-size:1.8rem}h2{margin:0 0 1rem;font-size:1.2rem}
.status,.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:1rem}
.status{margin-bottom:1.3rem}.status div,section{padding:1.25rem;background:white;border:1px solid #cbd8d8;border-radius:.7rem}
.status small,.status b{display:block}.status small{color:#506467}.status b{overflow-wrap:anywhere}
section{margin-bottom:1rem}.grid section{margin:0}.grid{margin-bottom:1rem}form{display:grid;gap:.85rem}
label{display:grid;gap:.25rem;font-weight:550}input{width:100%;padding:.65rem;border:1px solid #879e9e;border-radius:.35rem;font:inherit}
input:focus-visible,button:focus-visible{outline:3px solid #e99e39;outline-offset:2px}
button{justify-self:start;background:#076656;color:white;border:0;border-radius:.35rem;padding:.65rem 1rem;font:inherit;font-weight:650;cursor:pointer}
.hint{color:#506467;font-size:.88rem;margin:0}.notice{border-left:3px solid #16816f;padding:.5rem .8rem;background:#e1f2ed}
@media(max-width:42rem){.grid{grid-template-columns:1fr}.status{gap:.5rem}.status div{padding:.8rem}}
)CSS";
