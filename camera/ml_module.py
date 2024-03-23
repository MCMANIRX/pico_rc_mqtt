import cv2
import numpy as np
import time
import mediapipe as mp
from mediapipe.tasks import python
from mediapipe.tasks.python import vision

MARGIN = 10  # pixels
ROW_SIZE = 10  # pixels
FONT_SIZE = 1
FONT_THICKNESS = 1
TEXT_COLOR = (255, 0, 0)  # red


ObjectDetector = mp.tasks.vision.ObjectDetector
ObjectDetectorOptions = mp.tasks.vision.ObjectDetectorOptions
VisionRunningMode = mp.tasks.vision.RunningMode
DetectionResult = mp.tasks.components.containers.DetectionResult

detection_result= mp.tasks.components.containers.DetectionResult

def visualize(
    image,
    detection_result
) -> np.ndarray:
  """Draws bounding boxes on the input image and return it.
  Args:
    image: The input RGB image.
    detection_result: The list of all "Detection" entities to be visualize.
  Returns:
    Image with bounding boxes.
  """    
  stop_sign = False

  for detection in detection_result.detections:
    # Draw bounding_box
    if detection.categories[0].category_name == 'stop sign':
      stop_sign = True

      bbox = detection.bounding_box
      start_point = bbox.origin_x, bbox.origin_y
      end_point = bbox.origin_x + bbox.width, bbox.origin_y + bbox.height
      cv2.rectangle(image, start_point, end_point, TEXT_COLOR, 3)

      # Draw label and score
      category = detection.categories[0]
      category_name = category.category_name
      probability = round(category.score, 2)
      result_text = category_name + ' (' + str(probability) + ')'
      text_location = (MARGIN + bbox.origin_x,
                      MARGIN + ROW_SIZE + bbox.origin_y)
      cv2.putText(image, result_text, text_location, cv2.FONT_HERSHEY_PLAIN,
                  FONT_SIZE, TEXT_COLOR, FONT_THICKNESS)

  return image,stop_sign


# STEP 2: Create an ObjectDetector object.
MODEL_PATH = '2.tflite'#'efficientdet_lite0.tflite'

options = vision.ObjectDetectorOptions(base_options=python.BaseOptions(MODEL_PATH),
    max_results=30,
    #category_allowlist=['stop sign'], # extremely slow?
    running_mode=VisionRunningMode.IMAGE)
    #result_callback=print_result)

detector = vision.ObjectDetector.create_from_options(options)

def _look_at_frame(frame):

  mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=frame)
  
  detection_result = detector.detect(mp_image)
  #print(detection_result)
  stop_sign = False
  image_copy = frame
  if(detection_result!=None and hasattr(detection_result,'detections')): # 'None' test
      annotated_image,stop_sign = visualize(image_copy, detection_result)
  else: annotated_image = image_copy
  rgb_annotated_image = annotated_image
  return rgb_annotated_image,stop_sign

def look_at_frame(frame):
  #with vision.ObjectDetector.create_from_options(options) as detector:
  return _look_at_frame(frame)
