function modifyMindMap() {
    setTimeout(() => {
        document.querySelectorAll('*[class*="link:"]').forEach(node => {
            node.classList.forEach(className => {
                if (className.startsWith('link:')) {
                    console.log(node)
                    const tool = className.split(':')[1];
                    node.addEventListener('click', () => {
                        window.open(`${tool}.html`, '_self');
                    });
                }
            });
        });
    }, 1750)
}

window.addEventListener('load', modifyMindMap());
