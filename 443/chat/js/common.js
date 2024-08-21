document.addEventListener('DOMContentLoaded', () => {
    let currentid = [1];
    const modal = document.getElementById('myModal');
    const closeButton = document.querySelector('.close');
    const newMessageButton = document.getElementById('newMessage');
    const sendMessageButton = document.getElementById('sendMessage');
    function fetchBoardContent(page) {
        fetch(`https://chat.neuqboard.cn:1000/api/p=${page}`)
        .then(response => {
            if (!response.ok) throw new Error('Network response was not ok');
            return response.json();
        })
        .then(data => {
            const boardItemsDiv = document.getElementById('boardItems');
            boardItemsDiv.innerHTML = '';
            let newid = -1,sum=0,bj=0;
            data.forEach(item => {
                bj=1;
                const itemDiv = document.createElement('div');
                itemDiv.className = item.px=='0'?'board-item':'board-item2';
                sum++;
                if(sum%2==0)itemDiv.className+='_f9';
                itemDiv.style.marginLeft = `${item.px * 20}px`;
                if (item.px === '0') itemDiv.style.clear = 'both';
                const titleText = item.id < 0 ? `${item.title}` : `+ ${item.title}`;
                newid = item.id;
                itemDiv.innerHTML = `<strong id="s${item.id}"></strong> ${formatDate(item.date)}<button class="modal-trigger" data-id="${item.id}">Reply</button><br><strong class="item-title" data-id="${item.id}" id="tit${item.id}"></strong><div class="item-content" id="content-${item.id}"></div>`;
                boardItemsDiv.appendChild(itemDiv);
                document.getElementById(`s${item.id}`).textContent=item.name;
                document.getElementById(`tit${item.id}`).textContent=titleText;
            });
            if(bj==0)boardItemsDiv.innerHTML = 'No content available for this page.';
            if (newid != -1) currentid.push(newid);
            document.querySelectorAll('.item-title').forEach(titleElement => {
                titleElement.addEventListener('click', function() {
                    const itemId = this.dataset.id;
                    if (itemId < 0) {
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
                    document.getElementById('modalname').innerHTML = `Reply #${itemId<0?-itemId:itemId}`;
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

    closeButton.addEventListener('click', ()=>{modal.style.display = 'none';});
    newMessageButton.addEventListener('click', function() {
        document.getElementById('messageId').value = 1;
        document.getElementById('modalname').innerHTML = 'New Message';
        document.getElementById('messageTitle').value = '';
        document.getElementById('messageContent').value = '';
        modal.style.display = 'block';
    });
    sendMessageButton.addEventListener('click', function() {
        const title = document.getElementById('messageTitle').value;
        if (!title.trim()) {
            alert('Title cannot be empty.');
            return;
        }
        const content = document.getElementById('messageContent').value;
        const id = document.getElementById('messageId').value;
        const dataString = JSON.stringify(title) + '\0' + content + '\0' + id + '\0';
        fetch('https://chat.neuqboard.cn:1000/api/sendmessage', {
            method: 'POST',
            credentials: 'include', // 允许发送cookie
            headers: {'Content-Type': 'text/plain', 'Content-Length': dataString.length},
            body: dataString
        })
        .then(response => {
            if (!response.ok) throw new Error('Network response was not ok');  
            return response.text();
        })
        .then(data => {
            if (data == 'ok') location.reload();
            else alert(data);
        })
        .catch(error => {alert('Error sending message!');});
    });

    document.getElementById('newerButton').addEventListener('click', function() {
        if (currentid.length > 1) currentid.pop();
        if (currentid.length > 1) currentid.pop();
        fetchBoardContent(currentid[currentid.length-1]);
    });

    document.getElementById('olderButton').addEventListener('click', function() {
        fetchBoardContent(currentid[currentid.length-1]);
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

    function fetchContent(id, contentDiv) {
        fetch(`https://chat.neuqboard.cn:1000/api/con=${id}`)
        .then(response => {
            if (!response.ok) throw new Error('Network response was not ok');
            return response.text();
        })
        .then(data => {
            contentDiv.textContent = data;
            contentDiv.style.display = 'block';
        })
        .catch(error => {
            contentDiv.innerHTML = 'Error loading content!';
            contentDiv.style.display = 'block';
        });
    }
    fetchBoardContent(currentid[currentid.length-1]);
    function loaduser(){
        fetch('https://chat.neuqboard.cn:1000/api/user', {
            method: 'GET',
            credentials: 'include' // 允许发送cookie
        })
        .then(response => {
            if (!response.ok) throw new Error('Network response was not ok');
            return response.text();
        })
        .then(data => {
            document.getElementById('userStatus').textContent = '';
            if(data === "Not_Logged_In")
                document.getElementById('signin').style.display = 'inline';
            else {
                document.getElementById('islogin').style.display = 'inline';
                document.getElementById('username').textContent =data;
            }
        })
        .catch(error => {
            document.getElementById('userStatus').textContent = 'Error loading user status!';
        });
    }
    loaduser();
});