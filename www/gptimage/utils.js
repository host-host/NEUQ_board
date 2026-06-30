// 压缩 / 编码图片为 base64 data URL（修改模式上传源图用）
function compressImage(file, maxWidth = 1280, maxHeight = 1280, quality = 0.9) {
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
                resolve(canvas.toDataURL('image/jpeg', quality));
            };
            img.onerror = (err) => reject(err);
        };
        reader.onerror = (err) => reject(err);
    });
}

// 点击放大查看图片
function zoomImage(imgSrc) {
    let overlay = document.getElementById('imageZoomOverlay');
    if (!overlay) {
        overlay = document.createElement('div');
        overlay.id = 'imageZoomOverlay';
        overlay.style = `
            position: fixed; top: 0; left: 0; width: 100vw; height: 100vh;
            background: rgba(0,0,0,0.85); display: flex; align-items: center;
            justify-content: center; z-index: 99999; cursor: zoom-out;
            opacity: 0; transition: opacity 0.2s ease-in-out;`;
        overlay.onclick = () => {
            overlay.style.opacity = '0';
            setTimeout(() => overlay.style.display = 'none', 200);
        };
        const img = document.createElement('img');
        img.style = 'max-width:90%; max-height:90%; object-fit:contain; border-radius:8px; box-shadow:0 4px 20px rgba(0,0,0,0.5);';
        overlay.appendChild(img);
        document.body.appendChild(overlay);
    }
    overlay.querySelector('img').src = imgSrc;
    overlay.style.display = 'flex';
    setTimeout(() => overlay.style.opacity = '1', 10);
}

// 下载一张图片（支持 data URL 和普通 http 链接）
async function downloadImage(src, filename) {
    try {
        let blob;
        if (src.startsWith('data:')) {
            const res = await fetch(src);
            blob = await res.blob();
        } else {
            const res = await fetch(src);
            blob = await res.blob();
        }
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = filename || `image_${Date.now()}.png`;
        document.body.appendChild(a);
        a.click();
        a.remove();
        URL.revokeObjectURL(url);
    } catch (e) {
        // 跨域图片无法 fetch 时，退回新窗口打开
        window.open(src, '_blank');
    }
}

// 从一段文本里提取所有图片（markdown 图片、裸 data URL、裸图片直链）
function extractImagesFromText(text) {
    if (!text) return [];
    const images = [];
    const seen = new Set();
    const push = (u) => { if (u && !seen.has(u)) { seen.add(u); images.push(u); } };

    // 1. markdown 图片 ![alt](url)
    const mdImg = /!\[[^\]]*\]\((data:image\/[^)]+|https?:\/\/[^)\s]+)\)/g;
    let m;
    while ((m = mdImg.exec(text)) !== null) push(m[1]);

    // 2. 裸 base64 data URL
    const dataUrl = /data:image\/[a-zA-Z0-9.+-]+;base64,[A-Za-z0-9+/=]+/g;
    while ((m = dataUrl.exec(text)) !== null) push(m[0]);

    // 3. 裸图片直链
    const directUrl = /https?:\/\/[^\s)"']+\.(?:png|jpe?g|gif|webp)(?:\?[^\s)"']*)?/gi;
    while ((m = directUrl.exec(text)) !== null) push(m[0]);

    return images;
}

// 把文本里的图片标记去掉，剩下的当作模型的文字说明
function stripImagesFromText(text) {
    if (!text) return '';
    return text
        .replace(/!\[[^\]]*\]\((?:data:image\/[^)]+|https?:\/\/[^)\s]+)\)/g, '')
        .replace(/data:image\/[a-zA-Z0-9.+-]+;base64,[A-Za-z0-9+/=]+/g, '')
        .replace(/https?:\/\/[^\s)"']+\.(?:png|jpe?g|gif|webp)(?:\?[^\s)"']*)?/gi, '')
        .trim();
}
