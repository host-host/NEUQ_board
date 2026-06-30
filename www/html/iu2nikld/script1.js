var data,index,output;
function pre(x,i){
    return i%x==0?i-x:parseInt(index/x)*x;
}
function 均线(x,y){
    let s=0,i=index;
    for(let j=1;j<=y;j++){
        if(i>=0){
            s+=data[i].close;
            i=pre(x,i);
        }else return null;
    }
    return s/y;
}
function 前一分钟均线(x,y){
    let s=0,i=pre(x,index);
    for(let j=1;j<=y;j++){
        if(i>=0){
            s+=data[i].close;
            i=pre(x,i);
        }else return null;
    }
    return s/y;
}
function calma(x,y,data,index){
    if (isNaN(x)|| x < 1)throw new Error("x输入不合法");
    if (isNaN(y)|| y < 1)throw new Error("y输入不合法");
    let s=0;
    for(let i=1;i<=y;i++){
        if(index>=0){
            s+=data[index].close;
            index=pre(x,index);
        }else return null;
    }
    return s/y;
}
function 价格平均线多(x,y){
    x1=calma(x,3,data,index);
    x2=calma(x,3,data,pre(x,index));
    if(x1==null||x2==null)return null;
    return (x1-x2)/x1*y;
}
function 价格平均线空(x,y){
    return-价格平均线多(x,y);
}
function 输出(x){
    output.push(x);
}
function 股价移动速度(x,y){
    let s=0,ini=index;
    for(let i=1;i<=y;i++){
        s+=gyj(data,x,ini);
        ini=pre(x,ini);
    }
    if(isNaN(s))return null;
    return s/60*11000/y;
}
async function startBacktest() {// 主回测函数
    try {   
        document.getElementById('output').value = '正在计算...\n';
        let file = document.getElementById('fileInput').files[0];
        if (!file) throw new Error('请选择数据文件');
        let text = await file.text();
        let lines = text.split('\n').filter(l => l.trim());
        data=[{}];
        output = [];
        let position = null;
        let totalReturn = 0,tots=0;
        eval(`function openfunction(){`+document.getElementById(`open` ).value+`}`);
        eval(`function closefunction(){`+document.getElementById(`close`).value+`}`);
        for (const line of lines) {
            const parts = line.split('\t');
            if (parts.length !== 9) continue;
            const close = parseFloat(parts[5]);
            if(isNaN(close))continue;
            data.push({
                date: parts[0] + ' ' + parts[1],
                open: parseFloat(parts[2]),
                high: parseFloat(parts[3]),
                low: parseFloat(parts[4]),
                close: close,
            });
            index=data.length-1;
            if(parts[1].includes(2101)&&index%15!=1)output.push(`警告，数据可能不合法${data[index].date}`);
            if (!position) {
                let tmp=openfunction(index);
                if(tmp==='开多'||tmp==='开空'){
                    position = {price: data[index].close,direction: tmp};
                    output.push(`${data[index].date} ${tmp} ${data[index].close}`);
                }
            }
            if (position) {
                let tmp=closefunction(index);
                if(tmp==='平'){
                    const returnPct = position.direction === '开多'?data[index].close-position.price:position.price-data[index].close;
                    totalReturn += returnPct;
                    output.push(`${data[index].date} 平   ${data[index].close}`);
                    position = null;
                    tots++;
                }
            }
        }
        output.push(`总收益：${totalReturn}    总交易次数: ${tots}`);
        document.getElementById('output').value = output.join('\n');
    } catch (err) {
        document.getElementById('output').value += '错误: ' + err.message;
    }
    const tmp=document.getElementById('output');
    tmp.style.height = '0px';
    tmp.style.height = tmp.scrollHeight + 'px';
}