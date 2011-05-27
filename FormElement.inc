<?php

// $Id$

/**
 * @file
 *
 */
module_load_include('inc', 'php_lib', 'ReflectionHelpers');
module_load_include('inc', 'php_lib', 'ReadOnlyProtectedMembers');
module_load_include('inc', 'objective_forms', 'Utils');
//module_load_include('inc', 'objective_forms', 'FormControl');

/**
 * Form Element Class.
 */
class FormElement implements ArrayAccess {

  /**
   * Holds references to protected variables. Allows for external access.
   * 
   * @var array
   */
  protected $protected;
  /**
   * Child Form Elements
   * 
   * @var FormElement[]
   */
  public $children;
  /**
   * Child Form Controls.
   * 
   * @var FormControl[]
   */
  public $controls;

  /**
   * Creates a Empty Form Element.
   * 
   * @param array $controls
   * @param array $children
   */
  public function __construct(array $controls = NULL) {
    $this->controls = isset($controls) ? $controls : array();
    $this->children = array();
    $this->protected = new ReadOnlyProtectedMembers(array('parent' => NULL, 'hash' => spl_object_hash($this)));
    module_invoke_all('form_element_created', $this); // Some modules will want to register all created FormElements.
  }

  /**
   * Called when this object is destroyed.
   */
  public function __destruct() {
    module_invoke_all('form_element_destroyed', $this->hash); // Some modules will want to be notified of any destroyed FormElements.
  }

  /**
   * Clone this FormElement. 
   * 
   * Performs a deep clone on controls and children. The cloned element wont have a parent.
   */
  public function __clone() {
    $original_hash = $this->hash;
    $this->hash = spl_object_hash($this); // Maintain uniqueness.
    $this->parent = NULL; // The parent was unaware of this cloned bastard child of science...
    foreach ($this->controls as $key => $control) { // Clone Controls.
      $this->controls[$key] = clone $control;
    }
    foreach ($this->children as $key => $child) { // Clone Children.
      $this->children[$key] = clone $child;
    }
    module_invoke_all('form_element_cloned', $original_hash, $this); // Some modules will want to register all cloned FormElements.
  }

  /**
   * Takes a function applies it to each child.
   */
  public function eachChild($function) {
    $args = func_get_args();
    array_shift($args); // Remove function argument
    foreach ($this->children as $child) {
      array_unshift($child, $args);
      if (call_user_func_array($function, $args) == FALSE) {
        return;
      }
      array_shift($args);
    }
  }

  /**
   * Takes a function applies it to each control.
   */
  public function eachControl($function) {
    $args = func_get_args();
    array_shift($args); // Remove function argument
    foreach ($this->controls as $key => $control) {
      array_unshift($control, $args);
      array_unshift($key, $args);
      if (call_user_func_array($function, $args) == FALSE) {
        return;
      }
      array_shift($args); // $control
      array_shift($args); // $key
    }
  }

  /**
   * Returns an iterator that iterates over this elements decendants. Children and children of children.
   * 
   * @return iterator
   */
  public function eachDecendant($function) {
    $args = func_get_args();
    array_shift($args); // Remove function argument
    foreach ($this->children as $child) {
      if (call_user_func_array($function, $args) == FALSE) {
        return;
      }
    }
  }

  /**
   * 
   */
  public function __toString() {
    // Implement for debug purposes.
  }

  /**
   * Gets a child or control if found.
   * 
   * @param mixed $name 
   * @return mixed
   */
  public function __get($name) {
    if ($this->protected->exists($name)) {
      return $this->protected->$name;
    }
    else if ($this->offsetExists($name)) { // Child
      return $this->offsetGet($name);
    }
    else if ($this->offsetExists("#$name")) { // Control
      return $this->offsetGet("#$name");
    }
    return NULL;
  }

  /**
   * Sets a child or control.
   *  
   * @param mixed $name
   * @param mixed $value 
   */
  public function __set($name, $value) {
    if ($this->protected->exists($name)) {
      $this->protected->$name = $value;
    }
    else {
      $name = is_or_descends_from($value, 'FormElement') ? $name : "#$name";
      $this->offsetSet($name, $value);
    }
  }

  /**
   * Checks if a given control or child exists.
   * 
   * @param mixed $name 
   * 
   * @return boolean
   */
  public function __isset($name) {
    if ($this->protected->exists($name)) {
      return isset($this->protected->$name);
    }
    return $this->offsetExists($name);
  }

  /**
   * Removes a child or control if found.
   * 
   * @param mixed $name 
   */
  public function __unset($name) {
    if ($this->protected->exists($name)) {
      unset($this->protected->$name);
    }
    else if ($this->offsetExists($name)) { // Child
      $this->offsetUnset($name);
    }
    else if ($this->offsetExists("#$name")) { // Control
      $this->offsetUnset("#$name");
    }
  }

  /**
   * Checks to see if any child or control exists for the given offset.
   * 
   * @param mixed $offset 
   * @return boolean
   */
  public function offsetExists($offset) {
    $child_exists = isset($this->children[$offset]);
    $control_exists = isset($this->controls[$offset]);
    return $child_exists || $control_exists;
  }

  /**
   * Looks for a child or control for the the given offset.
   * 
   * @param mixed $offset 
   * @return mixed
   *   If found, this function will return either a child FormElement or a FormControl.
   */
  public function offsetGet($offset) {
    // Children
    $child_exists = array_key_exists($offset, $this->children);
    if ($child_exists) {
      return $this->children[$offset];
    }
    // Controls
    $control_exists = array_key_exists($offset, $this->controls);
    if ($control_exists) {
      return $this->controls[$offset];
    }
    return NULL;
  }

  /**
   * Sets a child or control defined by $value.
   * 
   * @param mixed $offset
   * @param mixed $value 
   *   Either a FormElement or a FormControl, all other values will be ignored.
   */
  public function offsetSet($offset, $value) {
    if (is_or_descends_from($value, 'FormElement')) {
      $this->adopt($offset, $value);
    }
    else if (element_property($offset)) {
      $this->controls[$offset] = $value;
    }
  }

  /**
   * Removes the child or control identified by $offset if found.
   * 
   * @param mixed $offset 
   */
  public function offsetUnset($offset) {
    if (isset($this->children[$offset])) {
      unset($this->children[$offset]);
    }
    else if (isset($this->controls[$offset])) {
      unset($this->controls[$offset]);
    }
  }

  /**
   * Sets a control to this form element.
   * 
   * @param mixed $offset
   * @param FormControl $control 
   */
  private function setControl($offset, FormControl $control) {
    if (isset($offset)) {
      var_dump($control);
      $this->controls[$offset] = $control;
    } // Controls are not indexed values EVAR!.
  }

  /**
   * Adopt the given child. Forcefully removing from its original home.
   * 
   * @param mixed $offse
   * @param FormElement $child
   */
  private function adopt($offset = NULL, FormElement $child) {
    $child->setParent($this);
    if (isset($offset)) {
      $this->children[$offset] = $child;
    }
    else {
      $this->children[] = $child;
    }
    $child->parent = $this;
  }

  /**
   * Sets this elements parent, ditching its previous parents.
   * 
   * @param FormElement
   */
  private function setParent(FormElement $parent) {
    $this->orphan();
    $this->parent = $parent;
  }

  /**
   * Removes this element from its parent element.
   * 
   * @return boolean
   *   TRUE if the element was orphaned FALSE otherwise.
   */
  public function orphan() {
    if (isset($this->parent)) {
      $was_orphaned = $this->parent->orphanChild($this);
      unset($this->parent);
      return $was_orphaned;
    }
    return FALSE;
  }

  /**
   * Removes the child element defined by $remove from this FormElement. 
   * 
   * @param FormElement $remove
   *   The child to remove
   * 
   * @return boolean
   *   TRUE if the child was removed, FALSE otherwise.
   */
  private function orphanChild(FormElement $remove) {
    foreach ($this->children as $index => $child) {
      if ($remove === $child) {
        unset($this->children[$index]);
        return TRUE;
      }
    }
    return FALSE;
  }

  /**
   * Get's the index of this element in its parent's children array if it has a parent.
   */
  public function getIndex() {
    if (isset($this->parent)) {
      foreach ($this->parent->children as $index => $child) {
        if ($child === $this) {
          return $index;
        }
      }
    }
    return NULL;
  }

  /**
   *
   * @return array
   */
  public function toArray() {
    $output = $this->controls;
    foreach ($this->children as $name => $child) {
      $output[$name] = $child->toArray();
    }
    return $output;
  }

}