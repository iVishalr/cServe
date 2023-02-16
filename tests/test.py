import requests
import time

response_time = 0
elapsed_time = 0
time_list = []

links = [
    "/assets/fonts/fontawesome-webfont.ttf",
    "/assets/fonts/fontawesome-webfont.woff",
    "/assets/fonts/fontawesome-webfont.woff2",
    "/images/bg.jpg",
    "/images/overlay.png",
    "/images/pic03.jpg",
    "/assets/css/font-awesome.min.css",
    "/images/pic02.jpg",
    "/assets/js/main.js",
    "/assets/js/util.js",
    "/assets/js/skel.min.js",
    "/assets/js/jquery.min.js",
    "/images/pic01.jpg",
    "/assets/css/main.css",
    "/index.html"
]

wall_start = time.time_ns() / 1000 / 1000
for i in range(1):
    start = time.time_ns() / 1000 / 1000
    for j in links[::-1]:
        r = requests.get(f"http://localhost:8082{j}")
        r.close()
    end = time.time_ns() / 1000 / 1000
    response_time = end - start
    elapsed_time += response_time
    print(f"Request - {i+1} : Time {response_time:.4f}ms;  Elapsed Time: {elapsed_time / 1000:.4f}s")
    time_list.append(end - start)
wall_end = time.time_ns() / 1000 / 1000

# print(f"Avg Time : {sum(time_list)/len(time_list)}")
# print(f"Wall Clock Time : {(wall_end - wall_start) / 1000 :.4f}s")
