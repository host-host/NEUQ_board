
// 添加条件模板
function addCondition(type) {
    const container = document.createElement('div');
    container.className = 'condition-row';
    
    const select_=`
        <select class="select1">
            <option value="ma">均线</option>
            <option value="prev_ma">前一日均线</option>
            <option value="number">数值</option>
            <option value="close">收盘价</option>
            <option value="open">开盘价</option>
            <option value="closeq">收盘价前一日</option>
            <option value="openq">开盘价前一日</option>
            <option value="volume">成交量的y倍</option>
            <option value="volumeq">成交量前一日的y倍</option>
            <option value="valaved">价格平均线y日多</option>
            <option value="valavek">价格平均线y日空</option>
            <option value="valavedq">价格平均线y日多(前一日)</option>
            <option value="valavekq">价格平均线y日空(前一日)</option>
            <option value="gjydsd">股价移动速度</option>
            <option value="print">输出</option>
        </select>
        <input type="txt" placeholder="x" class="input1">
        <input type="txt" placeholder="y" class="input1">`;
    const html = select_+`
        <select class="select2">
            <option value=">">></option>
            <option value="<"><</option>
            <option value=">=">>=</option>
            <option value="<="><=</option>
            <option value="=">=</option>
        </select>`+select_+`<button onclick="this.parentElement.remove()">删除</button>`;//placeholder="x" 
    
    container.innerHTML = html;
    document.getElementById(`${type}Conditions`).appendChild(container);
    // 监听 select 元素的 change 事件
    const selects = container.querySelectorAll('.select1');
    selects.forEach(select => {
        select.addEventListener('change', function() {
            const inputForMaPrevMa1 = this.nextElementSibling;
            const inputForMaPrevMa2 = inputForMaPrevMa1.nextElementSibling;
            const inputForMaPrevMa3 = inputForMaPrevMa2.nextElementSibling;
            let t1=["ma","prev_ma","open","high","low","valaved","valavek","valavedq","valavekq","gjydsd","volume","volumeq","open","openq","closeq"],bj=0,tmp=this.value;
            for(let i=0;i<t1.length;i++)if(tmp===t1[i])bj=1;
            inputForMaPrevMa1.style.visibility = bj===1?'visible':'hidden';
            if (tmp==="ma"||tmp==="prev_ma"||tmp==="number"||tmp=="valaved"||tmp=="valavek"||tmp==="valavedq"||tmp==="valavekq"||tmp=="gjydsd"||tmp==="volume"||tmp==="volumeq")
                inputForMaPrevMa2.style.visibility = 'visible';
            else inputForMaPrevMa2.style.visibility  = 'hidden';

            if(tmp==="print")inputForMaPrevMa3.style.visibility  = 'hidden';
            else inputForMaPrevMa3.style.visibility = 'visible';
        });
    });
}


var data;
// function maincalc(lines) {
//     data = [];
//     let closePrices = [];
//     let ema12 = null;
//     let ema26 = null;
//     let diffHistory = [];
//     let dea = null;

//     for (const line of lines) {
//         const parts = line.split('\t');
//         if (parts.length !== 9) continue;

//         const close = parseFloat(parts[5]);
//         if(isNaN(close))continue;
//         closePrices.push(close);

//         // 计算EMA12
//         if (closePrices.length >= 12) {
//             if (ema12 === null) {
//                 // 初始SMA计算
//                 const sum = closePrices.slice(0, 12).reduce((acc, val) => acc + val, 0);
//                 ema12 = sum / 12;
//             } else {
//                 // EMA递归计算
//                 const multiplier = 2 / (12 + 1);
//                 ema12 = (close * multiplier) + (ema12 * (1 - multiplier));
//             }
//         }

//         // 计算EMA26
//         if (closePrices.length >= 26) {
//             if (ema26 === null) {
//                 const sum = closePrices.slice(0, 26).reduce((acc, val) => acc + val, 0);
//                 ema26 = sum / 26;
//             } else {
//                 const multiplier = 2 / (26 + 1);
//                 ema26 = (close * multiplier) + (ema26 * (1 - multiplier));
//             }
//         }

//         // 计算DIFF
//         let diff = null;
//         if (ema12 !== null && ema26 !== null) {
//             diff = ema12 - ema26;
//             diffHistory.push(diff);
//         }

//         // 计算DEA（基于DIFF的9日EMA）
//         if (diff !== null && diffHistory.length >= 9) {
//             if (dea === null) {
//                 // 初始SMA计算l
//                 const sum = diffHistory.slice(0, 9).reduce((acc, val) => acc + val, 0);
//                 dea = sum / 9;
//             } else {
//                 const multiplier = 2 / (9 + 1);
//                 dea = (diff * multiplier) + (dea * (1 - multiplier));
//             }
//         }

//         // 计算MACD柱状图（通常为(DIFF - DEA)*2）
//         const macd = (diff !== null && dea !== null) ? (diff - dea) * 2 : null;

//         data.push({
//             date: parts[0] + ' ' + parts[1],
//             open: parseFloat(parts[2]),
//             high: parseFloat(parts[3]),
//             low: parseFloat(parts[4]),
//             close: close,
//             ema12: ema12 !== null ? Number(ema12.toFixed(4)) : null, // 保留4位小数
//             ema26: ema26 !== null ? Number(ema26.toFixed(4)) : null,
//             diff: diff !== null ? Number(diff.toFixed(4)) : null,
//             dea: dea !== null ? Number(dea.toFixed(4)) : null,
//             macd: macd !== null ? Number(macd.toFixed(4)) : null
//         });
//     }
// }
var addtext;
async function startBacktest() {// 主回测函数
    try {
        document.getElementById('output').value = '正在计算...\n';
        const file = document.getElementById('fileInput').files[0];
        if (!file) throw new Error('请选择数据文件');
        const text = await file.text();
        const lines = text.split('\n').filter(l => l.trim());
        data=[];
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
                volume: parseFloat(parts[6]),
            });
        }
        let position = null;
        let totalReturn = 0,tots=0;
        const output = [];
        if(!data[0].date.includes(' 2101'))output.push("警告，数据可能不合法");
        const mustclose=document.getElementById(`mustclose`).value.split(',');
        let time1=document.getElementById(`time1`).value,time2=document.getElementById(`time2`).value;
        time1=parseInt(time1);
        time2=parseInt(time2);
        for (let i =0; i < data.length; i++) {// 处理每日数据
            const current = data[i];
            let bj=0;
            for(let j=0;j<mustclose.length;j++)
                if(data[i].date.includes(mustclose[j])&&mustclose[j]!='')bj=1;
            if (position &&(checkConditions('close', data,i)||bj==1) ) {// 检查平仓条件
                if(!isNaN(time2)&&(i+1)%time2!=0)continue;
                const returnPct = position.direction === 'long'?current.close-position.price:position.price-current.close;
                totalReturn += returnPct;
                output.push(`${current.date} 平   ${current.close}`+addtext);
                position = null;
                tots++;
            }else if (!position && checkConditions('open', data,i)&&bj==0) {// 检查开仓条件
                if(!isNaN(time1)&&(i+1)%time1!=0)continue;
                position = {price: current.close,direction: document.getElementById('direction').value};
                output.push(`${current.date} ${position.direction==='long'?'开多':'开空'} ${current.close}`+addtext);
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
function checkConditions(type, data, index) {// 条件检查函数
    addtext='';
    const conditions = Array.from(document.getElementById(`${type}Conditions`).querySelectorAll('.condition-row'));
    for (const cond of conditions) {
        const inputs = cond.querySelectorAll('select, input');
        const [xSelect, xInput1,xInput2, opSelect, ySelect, yInput1, yInput2] = inputs;
        
        
        const operator = opSelect.value;
        const xVal = getOperandValue(xSelect, xInput1,xInput2, data, index);
        const yVal = getOperandValue(ySelect, yInput1,yInput2, data, index);
        
        if(xSelect.value=='print'){
            addtext+=' '+yVal;
            continue;
        }
        if (xVal === null || yVal === null) return false;
        
        let conditionMet;
        switch (operator) {
            case '>': conditionMet = xVal > yVal; break;
            case '<': conditionMet = xVal < yVal; break;
            case '>=': conditionMet = xVal >= yVal; break;
            case '<=': conditionMet = xVal <= yVal; break;
            case '=': conditionMet = xVal == yVal; break;
            default: conditionMet = false;
        }
        if (!conditionMet) return false;
    }
    return true;
}
function pre(x,index){
    return (index+1)%x==0?index-x:parseInt(index/x)*x-1;
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
function gyj(data,x,index){
    let tmp=pre(x,index);
    if(isNaN(tmp)||tmp<0)return NaN;
    return data[index].close-data[tmp].close;
}
function gjydsd(data,x,y,index){
    let s=0;
    for(let i=1;i<=y;i++){
        s+=gyj(data,x,index);
        index=pre(x,index);
    }
    if(isNaN(s))return null;
    return s/60*11000/y;
}
function getOperandValue(select, input1,input2, data, index) {
    let tmp,x,y,x1,x2;
    switch(select.value){
        case 'ma':return calma(parseInt(input1.value),parseInt(input2.value),data,index);
        case 'prev_ma':
            x=parseInt(input1.value);
            if (isNaN(x)|| x < 1)throw new Error("x输入不合法");
            return calma(parseInt(input1.value),parseInt(input2.value),data,pre(x,index));
        case 'number':
            y = parseFloat(input2.value);
            if (isNaN(y))throw new Error("y输入不合法");
            return y;
        case 'close': return data[index].close;
        case 'valaved':
            x=parseInt(input1.value);
            if (isNaN(x)|| x < 1)throw new Error("x输入不合法");
            y=parseInt(input2.value);
            if (isNaN(y)|| y < 1)throw new Error("y输入不合法");
            x1=calma(x,y,data,index);
            x2=calma(x,y,data,pre(x,index));
            if(x1==null||x2==null)return null;
            return (x1-x2)/x1*10000;
        case 'valavek':
            x=parseInt(input1.value);
            if (isNaN(x)|| x < 1)throw new Error("x输入不合法");
            y=parseInt(input2.value);
            if (isNaN(y)|| y < 1)throw new Error("y输入不合法");
            x1=calma(x,y,data,index);
            x2=calma(x,y,data,pre(x,index));
            if(x1==null||x2==null)return null;
            return (x2-x1)/x1*10000;
        case 'valavedq':
            x=parseInt(input1.value);
            if (isNaN(x)|| x < 1)throw new Error("x输入不合法");
            y=parseInt(input2.value);
            if (isNaN(y)|| y < 1)throw new Error("y输入不合法");
            x1=calma(x,y,data,pre(x,index));
            x2=calma(x,y,data,pre(x,pre(x,index)));
            if(x1==null||x2==null)return null;
            return (x1-x2)/x1*10000;
        case 'valavekq':
            x=parseInt(input1.value);
            if (isNaN(x)|| x < 1)throw new Error("x输入不合法");
            y=parseInt(input2.value);
            if (isNaN(y)|| y < 1)throw new Error("y输入不合法");
            x1=calma(x,y,data,pre(x,index));
            x2=calma(x,y,data,pre(x,pre(x,index)));
            if(x1==null||x2==null)return null;
            return (x2-x1)/x1*10000;
        case 'gjydsd':
            x=parseInt(input1.value);
            if (isNaN(x)|| x < 1)throw new Error("x输入不合法");
            y=parseInt(input2.value);
            if (isNaN(y)|| y < 1)throw new Error("y输入不合法");
            return gjydsd(data,x,y,index);
        case 'volume':
            x=parseInt(input1.value);
            if (isNaN(x)|| x < 1)throw new Error("x输入不合法");
            y1=parseFloat(input2.value);
            if (isNaN(y1))throw new Error("y输入不合法");
            y=0;
            for(let i=0;i<x;i++){
                let tmp=index-i;
                if(tmp<0)return null;
                y+=data[tmp].volume;
            }
            return y*y1;
        case 'volumeq':
            x=parseInt(input1.value);
            if (isNaN(x)|| x < 1)throw new Error("x输入不合法");
            y1=parseFloat(input2.value);
            if (isNaN(y1))throw new Error("y输入不合法");
            y=0;
            for(let i=0;i<x;i++){
                let tmp=index-i-x;
                if(tmp<0)return null;
                y+=data[tmp].volume;
            }
            return y*y1;
        case 'open':
            x=parseInt(input1.value);
            if (isNaN(x)|| x < 1)throw new Error("x输入不合法");
            if(index-x+1<0)return null;
            return data[index-x+1].open;
            return y;
        case 'openq':
            x=parseInt(input1.value);
            if (isNaN(x)|| x < 1)throw new Error("x输入不合法");
            if(index-x+1-x<0)return null;
            return data[index-x+1-x].open;
            return y;
        case 'closeq':
            x=parseInt(input1.value);
            if (isNaN(x)|| x < 1)throw new Error("x输入不合法");
            if(index-x<0)return null;
            return data[index-x].close;
        default:return null;
    }
}