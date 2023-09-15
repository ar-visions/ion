import cv2
import tkinter as tk
from PIL import Image, ImageTk

class WebcamApp:
    def __init__(self, window, window_title):
        self.window = window
        self.window.title(window_title)
        
        self.video_source = 0  # can change this if you have multiple webcams
        self.vid = cv2.VideoCapture(self.video_source)
        
        self.canvas = tk.Canvas(window, width=self.vid.get(cv2.CAP_PROP_FRAME_WIDTH), height=self.vid.get(cv2.CAP_PROP_FRAME_HEIGHT))
        self.canvas.pack()
        
        self.btn_screenshot = tk.Button(window, text="Screenshot", width=10, command=self.screenshot)
        self.btn_screenshot.pack(pady=20, side=tk.LEFT)
        
        self.btn_quit = tk.Button(window, text="Quit", width=10, command=window.quit)
        self.btn_quit.pack(pady=20, side=tk.RIGHT)

        self.update()
        self.window.mainloop()

    def update(self):
        ret, frame = self.vid.read()
        if ret:
            self.photo = ImageTk.PhotoImage(image=Image.fromarray(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)))
            self.canvas.create_image(0, 0, image=self.photo, anchor=tk.NW)
        self.window.after(10, self.update)

    def screenshot(self):
        ret, frame = self.vid.read()
        if ret:
            cv2.imwrite("screenshot.jpg", frame)

    def __del__(self):
        if self.vid.isOpened():
            self.vid.release()

root = tk.Tk()
app = WebcamApp(root, "Webcam App")
