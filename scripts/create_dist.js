#!/usr/bin/env node

// Copyright 2017 Cheng Zhao. All rights reserved.
// Use of this source code is governed by the license that can be found in the
// LICENSE file.

const {version, targetCpu, targetOs, execSync} = require('./common')

const fs = require('fs')
const path = require('path')
const JSZip = require('./libs/jszip')

// C++ static library.
const cppFiles = {
  linux: [
    'libyue.a',
  ],
  mac: [
    'libyue.a',
  ],
  win: [
    'libyue.lib',
  ],
}

// Lua loadable module.
const luaFiles = {
  linux: [
    'yue.so',
  ],
  mac: [
    'yue.so',
  ],
  win: [
  ],
}

// Independent executable.
const exeFiles = {
  linux: [
    'yue',
  ],
  mac: [
    'yue',
  ],
  win: [
    'yue.exe',
  ],
}

// Strip binaries for Linux.
if (targetOs == 'linux') {
  const list = cppFiles.linux.concat(luaFiles.linux).concat(exeFiles.linux)
  for (const file of list)
    strip(`out/Release/${file}`)
}

generateZip('libyue', cppFiles)
generateZip('yue', exeFiles)
if (targetOs != 'win')
  generateZip('lua_yue_lua_5.3', luaFiles)

// Generate zip for docs, but only do it for linux/x64 when running on CI, in
// order avoid uploading docs for multiple times.
if (process.env.CI != 'true' || (targetOs == 'linux' && targetCpu == 'x64')) {
  execSync('node ./scripts/create_docs.js')
  addFileToZip(new JSZip(), 'out/Release/gen/docs', 'out/Release/gen/docs')
    .generateNodeStream({streamFiles:true})
    .pipe(fs.createWriteStream(`out/Release/yue_docs_${version}.zip`))
}

function generateZip(name, list) {
  const zipname = `${name}_${version}_${targetOs}_${targetCpu}`
  let zip = new JSZip()
  for (let file of list[targetOs])
    addFileToZip(zip, `out/Release/${file}`, 'out/Release')
  zip.generateNodeStream({streamFiles:true})
     .pipe(fs.createWriteStream(`out/Release/${zipname}.zip`))
}

function addFileToZip(zip, file, base) {
  const stat = fs.statSync(file)
  if (stat.isDirectory()) {
    const subfiles = fs.readdirSync(file)
    for (let sub of subfiles)
      addFileToZip(zip, `${file}/${sub}`, base)
  } else if (stat.isFile()) {
    const filename = path.relative(base, file)
    zip.file(filename, fs.readFileSync(file))
  }
  return zip
}

function strip(file) {
  // TODO(zcbenz): Use |file| command to determine type.
  if (!file.endsWith('.so') && path.basename(file) != 'yue')
    return
  // TODO(zcbenz): Copy the debug symbols out before striping.
  const strip = targetCpu.startsWith('arm') ? 'arm-linux-gnueabihf-strip'
                                            : 'strip'
  execSync(`${strip} ${file}`)
}
