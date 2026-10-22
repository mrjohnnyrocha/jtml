// Independent expected outcomes for parser/binder role collisions. These are
// supplemental cases, not edits to the PR24 semantic expectations or catalog.
const names=['title','style','class','value','id'];
const tails=['data-label "marker"','aria-label "accessible"','style "color: red"',
 'data-label "marker" aria-label "accessible" style "color: red"',
 'disabled','click step','data-label="marker" aria-label "accessible"'];
module.exports=names.flatMap(name=>tails.map((tail,index)=>({
 id:`attribute-${name}-${index}`,fields:['result'],steps:[],checks:[],
 source:`jtml 2\nmake Probe\n  let ${name} = "Revenue"\n  let items = [1, 2]\n  let index = 0\n  get result = items[(index)] + (1)\n  when step\n    index = 1 - index\n  box class "probe"\n    p class "out-result" result\n    ${tail.startsWith('click ')?'button':'h2'} ${name} ${tail}\npage\n  Probe\n`,
 views:{heading:tail.startsWith('click ')?'button':'h2'},expected:[[{result:2}]],expectedViews:[[{heading:['Revenue']}]]
})));

// Parameter collisions exercise the exact reported regression independently.
module.exports.push(...names.map(name=>({
 id:`parameter-${name}`,fields:['result'],steps:[],checks:[],
 source:`jtml 2\nmake Probe ${name}\n  let result = 2\n  box class "probe"\n    p class "out-result" result\n    h2 ${name} data-label "marker" aria-label "accessible" style "color: red"\npage\n  Probe "Revenue"\n`,
 views:{heading:'h2'},expected:[[{result:2}]],expectedViews:[[{heading:['Revenue']}]]
})));

