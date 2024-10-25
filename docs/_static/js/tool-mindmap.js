window.addEventListener('load', () => {
    document.querySelectorAll('*[class*="link:"]').forEach(node => {
        node.classList.forEach(className => {
            if (className.startsWith('link:')) {
                console.log(node)
                const tool = className.split(':')[1];
                node.addEventListener('click', () => {
                    window.open(`./${tool}.html`, '_self');
                });
             }
        });
    });
});
