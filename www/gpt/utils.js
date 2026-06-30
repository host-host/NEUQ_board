function updateUrlParam(id) {//更新 URL 参数
    const url = new URL(window.location.href);
    if(id)url.searchParams.set('id', id);
    else url.searchParams.delete('id');
    window.history.pushState({}, '', url.toString());
}
function loadScript(src) {//动态加载 JS 脚本
    return new Promise((resolve, reject) => {
        const script = document.createElement('script');
        script.src = src;
        script.onload = resolve;
        script.onerror = reject;
        document.head.appendChild(script);
    });
}
async function extractTextFromPDF(arrayBuffer) {//提取 PDF 文本
    if (!window.pdfjsLib) {
        await loadScript('/cdn/cdnjs.cloudflare.com/ajax/libs/pdf.js/3.4.120/pdf.min.js');
        pdfjsLib.GlobalWorkerOptions.workerSrc = '/cdn/cdnjs.cloudflare.com/ajax/libs/pdf.js/3.4.120/pdf.worker.min.js';
    }
    const loadingTask = pdfjsLib.getDocument({ data: arrayBuffer });
    const pdf = await loadingTask.promise;
    let fullText = '';
    for (let i = 1; i <= pdf.numPages; i++) {
        const page = await pdf.getPage(i);
        const textContent = await page.getTextContent();
        const pageText = textContent.items.map(item => item.str).join(' ');
        fullText += pageText + '\n';
    }
    return fullText;
}
async function extractTextFromDocx(arrayBuffer) {//提取 Word 文本
    if (!window.mammoth) {
        await loadScript('/cdn/cdnjs.cloudflare.com/ajax/libs/mammoth/1.6.0/mammoth.browser.min.js');
    }
    const result = await mammoth.extractRawText({ arrayBuffer: arrayBuffer });
    return result.value;
}
async function extractTextFromXlsx(arrayBuffer) {//提取 Excel 文本，逐个工作表转为 CSV
    if (!window.XLSX) {
        await loadScript('/cdn/cdnjs.cloudflare.com/ajax/libs/xlsx/0.18.5/xlsx.full.min.js');
    }
    const workbook = XLSX.read(arrayBuffer, { type: 'array' });
    let fullText = '';
    workbook.SheetNames.forEach(sheetName => {
        const sheet = workbook.Sheets[sheetName];
        const csv = XLSX.utils.sheet_to_csv(sheet);
        if (csv.trim()) fullText += `# ${sheetName}\n${csv}\n\n`;
    });
    return fullText;
}
function compressImage(file, maxWidth = 1200, maxHeight = 1200, quality = 0.8) {//压缩图片
    return new Promise((resolve, reject) => {
        const reader = new FileReader();
        reader.readAsDataURL(file);
        reader.onload = (e) => {
            const img = new Image();
            img.src = e.target.result;
            img.onload = () => {
                let width = img.width;
                let height = img.height;
                if (width > maxWidth || height > maxHeight) {
                    if (width > height) {
                        height = Math.round((height * maxWidth) / width);
                        width = maxWidth;
                    } else {
                        width = Math.round((width * maxHeight) / height);
                        height = maxHeight;
                    }
                }
                const canvas = document.createElement('canvas');
                canvas.width = width;
                canvas.height = height;
                const ctx = canvas.getContext('2d');
                ctx.drawImage(img, 0, 0, width, height);
                const dataUrl = canvas.toDataURL('image/jpeg', quality);
                resolve(dataUrl);
            };
            img.onerror = (err) => reject(err);
        };
        reader.onerror = (err) => reject(err);
    });
}
function scrollToBottom() {//滚动到聊天底部
    const view = document.querySelector('.chat-box-viewport');
    view.scrollTop = view.scrollHeight;
}
function zoomImage(imgSrc) {//点击放大图片
    let overlay = document.getElementById('imageZoomOverlay');
    if (!overlay) {
        overlay = document.createElement('div');
        overlay.id = 'imageZoomOverlay';
        overlay.style = `
            position: fixed;
            top: 0; left: 0; width: 100vw; height: 100vh;
            background: rgba(0, 0, 0, 0.85);
            display: flex; align-items: center; justify-content: center;
            z-index: 99999; cursor: zoom-out;
            opacity: 0; transition: opacity 0.2s ease-in-out;
        `;
        overlay.onclick = () => {
            overlay.style.opacity = '0';
            setTimeout(() => overlay.style.display = 'none', 200);
        };
        const img = document.createElement('img');
        img.style = 'max-width: 90%; max-height: 90%; object-fit: contain; border-radius: 8px; box-shadow: 0 4px 20px rgba(0,0,0,0.5);';
        overlay.appendChild(img);
        document.body.appendChild(overlay);
    }
    const img = overlay.querySelector('img');
    img.src = imgSrc;
    overlay.style.display = 'flex';
    setTimeout(() => overlay.style.opacity = '1', 10);
}