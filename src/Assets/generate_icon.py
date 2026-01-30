"""生成应用程序图标"""
from PIL import Image, ImageDraw

def create_icon():
    sizes = [16, 32, 48, 64, 128, 256]
    images = []
    
    for size in sizes:
        # 创建紫色背景
        img = Image.new('RGBA', (size, size), (99, 102, 241, 255))
        draw = ImageDraw.Draw(img)
        
        # 画圆角矩形效果 (简单用圆形蒙版)
        # 画闪电 - 金色
        lightning_color = (251, 191, 36, 255)
        
        # 闪电坐标点 (相对于size)
        points = [
            (size * 0.6, size * 0.08),   # 顶部右
            (size * 0.35, size * 0.45),  # 中间左上
            (size * 0.5, size * 0.45),   # 中间中
            (size * 0.3, size * 0.92),   # 底部
            (size * 0.65, size * 0.5),   # 中间右下
            (size * 0.5, size * 0.5),    # 中间中下
        ]
        
        draw.polygon(points, fill=lightning_color)
        images.append(img)
    
    # 保存为 ICO (多尺寸)
    images[-1].save(
        r'C:\Repos\API-Detector\src\Assets\icon.ico',
        format='ICO',
        sizes=[(s, s) for s in sizes],
        append_images=images[:-1]
    )
    print(f'✓ 图标已生成: icon.ico ({len(sizes)} 种尺寸)')

if __name__ == '__main__':
    create_icon()
