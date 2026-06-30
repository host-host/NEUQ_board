document.addEventListener('DOMContentLoaded', () => {
    let currentid = [1];
    const modal = document.getElementById('myModal');
    const closeButton = document.querySelector('.close');
    const newMessageButton = document.getElementById('newMessage');
    const sendMessageButton = document.getElementById('sendMessage');
    
    // 获取帖子列表 (POST JSON)
    function fetchBoardContent(page) {
        fetch('/api/chat_list', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ id: String(page) })
        })
        .then(response => {
            if (!response.ok) throw new Error('Network response was not ok');
            return response.json();
        })
        .then(data => {
            const boardItemsDiv = document.getElementById('boardItems');
            boardItemsDiv.innerHTML = '';
            let newid = -1, sum = 0, bj = 0;
            
            data.forEach(item => {
                bj = 1;
                const itemDiv = document.createElement('div');
                itemDiv.className = item.px === 0 ? 'board-item' : 'board-item2';
                sum++;
                if(sum % 2 === 0) itemDiv.className += '_f9';
                itemDiv.style.marginLeft = `${item.px * 20}px`;
                if (item.px === 0) itemDiv.style.clear = 'both';
                
                const titleText = `${item.title}`;
                newid = item.id;
                
                if (item.empty) {
                    itemDiv.innerHTML = `<strong id="s${item.id}"></strong> ${formatDate(item.date)}<button class="modal-trigger" data-id="${item.id}">Reply</button><br><div class="item-title" data-id="${item.id}" id="tit${item.id}"></div><div class="item-content" id="content-${item.id}"></div>`;
                } else {
                    itemDiv.innerHTML = `<strong id="s${item.id}"></strong> ${formatDate(item.date)}<button class="modal-trigger" data-id="${item.id}">Reply</button><br><strong class="item-add">+ </strong><div class="item-title" data-id="${item.id}" id="tit${item.id}"></div><div class="item-content" id="content-${item.id}"></div>`;
                }
                boardItemsDiv.appendChild(itemDiv);
                document.getElementById(`s${item.id}`).textContent = item.name;
                document.getElementById(`tit${item.id}`).textContent = titleText;
            });
            
            if(bj === 0) boardItemsDiv.innerHTML = 'No content available for this page.';
            if (newid !== -1) currentid.push(newid);
            
            document.querySelectorAll('.item-title').forEach(titleElement => {
                titleElement.addEventListener('click', function() {
                    const itemId = this.dataset.id;
                    if (Number(itemId) < 0) {
                        alert('No content available for this item.');
                        return;
                    }
                    const contentDiv = this.nextElementSibling;
                    if (contentDiv.style.display === 'none' || contentDiv.style.display === '') {
                        fetchContent(itemId, contentDiv);
                    } else {
                        contentDiv.style.display = 'none';
                    }
                });
            });
            
            document.querySelectorAll('.modal-trigger').forEach(button => {
                button.addEventListener('click', function() {
                    const itemId = this.dataset.id;
                    document.getElementById('modalname').innerHTML = `Reply #${Number(itemId) < 0 ? -itemId : itemId}`;
                    document.getElementById('messageId').value = itemId;
                    document.getElementById('messageTitle').value = '';
                    document.getElementById('messageContent').value = '';
                    modal.style.display = 'block';
                });
            });
        })
        .catch(error => {
            document.getElementById('boardItems').textContent = 'Error loading board content!';
        });
    }

    closeButton.addEventListener('click', () => { modal.style.display = 'none'; });
    newMessageButton.addEventListener('click', function() {
        document.getElementById('messageId').value = "1";
        document.getElementById('modalname').innerHTML = 'New Message';
        document.getElementById('messageTitle').value = '';
        document.getElementById('messageContent').value = '';
        modal.style.display = 'block';
    });
    
    // 发送消息 (POST JSON)
    sendMessageButton.addEventListener('click', function() {
        const title = document.getElementById('messageTitle').value;
        if (!title.trim()) {
            alert('Title cannot be empty.');
            return;
        }
        const content = document.getElementById('messageContent').value;
        const id = document.getElementById('messageId').value;
        
        // 打包为 JSON，不再采用 \0 拼接
        const requestPayload = {
            title: title,
            content: content,
            parentId: String(id)
        };
        
        fetch('/api/chat_send', {
            method: 'POST',
            credentials: 'include',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(requestPayload)
        })
        .then(response => {
            if (!response.ok) throw new Error('Network response was not ok');  
            return response.json();
        })
        .then(data => {
            if (data.status === 'ok') {
                location.reload();
            } else {
                alert(data.message || 'An error occurred.');
            }
        })
        .catch(error => { alert('Error sending message!'); });
    });

    document.getElementById('newerButton').addEventListener('click', function() {
        if (currentid.length > 1) currentid.pop();
        if (currentid.length > 1) currentid.pop();
        fetchBoardContent(currentid[currentid.length - 1]);
    });

    document.getElementById('olderButton').addEventListener('click', function() {
        fetchBoardContent(currentid[currentid.length - 1]);
    });

    function formatDate(timestamp) {
        const date = new Date(timestamp * 1000);
        const year = date.getFullYear();
        const month = String(date.getMonth() + 1).padStart(2, '0');
        const day = String(date.getDate()).padStart(2, '0');
        const hour = String(date.getHours()).padStart(2, '0');
        const minute = String(date.getMinutes()).padStart(2, '0');
        const second = String(date.getSeconds()).padStart(2, '0');
        return `${year}-${month}-${day} ${hour}:${minute}:${second}`;
    }

    // 获取内容 (POST JSON)
    function fetchContent(id, contentDiv) {
        fetch('/api/chat_content', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ id: String(id) })
        })
        .then(response => {
            if (!response.ok) throw new Error('Network response was not ok');
            return response.json();
        })
        .then(data => {
            if (data.status === 'ok') {
                contentDiv.textContent = data.content;
                contentDiv.style.display = 'block';
            }
        })
        .catch(error => {
            contentDiv.innerHTML = 'Error loading content!';
            contentDiv.style.display = 'block';
        });
    }
    
    fetchBoardContent(currentid[currentid.length - 1]);
    
    function loaduser(){
        fetch('/api/user', {
            method: 'GET',
            credentials: 'include'
        })
        .then(response => {
            if (!response.ok) throw new Error('Network response was not ok');
            return response.json();
        })
        .then(data => {
            document.getElementById('userStatus').textContent = '';
            if(!data.name)
                document.getElementById('signin').style.display = 'inline';
            else {
                document.getElementById('islogin').style.display = 'inline';
                document.getElementById('username').textContent = data.name;
            }
        })
        .catch(error => {
            document.getElementById('userStatus').textContent = 'Error loading user status!';
        });
    }
    loaduser();
});
